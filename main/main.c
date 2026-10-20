#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "soc/interrupts.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "SYS_MSG";

//APUNTADOR A REGISTROS HWREG Y HWREG
#define HWREG32_ADDR(X)		((volatile uint32_t *)(X))
#define HWREG32(X)			(*(HWREG32_ADDR(X)))

//REGISTROS GPIO
#define GPIO_OUT_W1TS 		(HWREG32(0x3FF44008))
#define GPIO_OUT_W1TC 		(HWREG32(0x3FF4400C))

#define GPIO_PIN22_REG_ADDR (HWREG32_ADDR(0x3FF440E0))
#define GPIO_PIN23_REG_ADDR	(HWREG32_ADDR(0x3FF440E4))
#define GPIO_PIN22_REG 		(HWREG32(GPIO_PIN22_REG_ADDR))
#define GPIO_PIN23_REG 		(HWREG32(GPIO_PIN23_REG_ADDR))

#define IO_MUX_GPIO22_REG_ADDR	(HWREG32_ADDR(0x3FF49080))
#define IO_MUX_GPIO23_REG_ADDR	(HWREG32_ADDR(0x3FF4908C))

#define IO_MUX_GPIO22_REG	(HWREG32(IO_MUX_GPIO22_REG_ADDR))
#define IO_MUX_GPIO23_REG	(HWREG32(IO_MUX_GPIO23_REG_ADDR))

#define GPIO_ENABLE_W1TS 	(HWREG32(0x3FF44024))
#define GPIO_ENABLE_W1TC 	(HWREG32(0x3FF44028))

#define GPIO_IN				(HWREG32(0x3FF4403C))	//Nivel de los pines (presionado/soltado)
#define GPIO_STATUS			(HWREG32(0x3FF44044))	//Registro de status de interrupciones
#define GPIO_STATUS_W1TS	(HWREG32(0x3FF44048))
#define GPIO_STATUS_W1TC	(HWREG32(0x3FF4404C))

//REGISTROS TIMERS
#define TIMG0_BASE       0x3FF5F000
#define T0CONFIG      (*(volatile uint32_t *)(TIMG0_BASE + 0x000))
#define T0LO          (*(volatile uint32_t *)(TIMG0_BASE + 0x004))
#define T0HI          (*(volatile uint32_t *)(TIMG0_BASE + 0x008))
#define T0UPDATE      (*(volatile uint32_t *)(TIMG0_BASE + 0x00C))
#define T0ALARMLO     (*(volatile uint32_t *)(TIMG0_BASE + 0x010))
#define T0ALARMHI     (*(volatile uint32_t *)(TIMG0_BASE + 0x014))
#define T0LOADLO      (*(volatile uint32_t *)(TIMG0_BASE + 0x018))
#define T0LOADHI      (*(volatile uint32_t *)(TIMG0_BASE + 0x01C))
#define T0LOAD        (*(volatile uint32_t *)(TIMG0_BASE + 0x020))

#define TIMG0_INT_ENA   (*(volatile uint32_t *)(TIMG0_BASE + 0x098))
#define TIMG0_INT_RAW   (*(volatile uint32_t *)(TIMG0_BASE + 0x09C))
#define TIMG0_INT_ST    (*(volatile uint32_t *)(TIMG0_BASE + 0x0A0))
#define TIMG0_INT_CLR   (*(volatile uint32_t *)(TIMG0_BASE + 0x0A4))

#define T1CONFIG      (*(volatile uint32_t *)(TIMG0_BASE + 0x024))
#define T1LO          (*(volatile uint32_t *)(TIMG0_BASE + 0x028))
#define T1HI          (*(volatile uint32_t *)(TIMG0_BASE + 0x02C))
#define T1UPDATE      (*(volatile uint32_t *)(TIMG0_BASE + 0x030))
#define T1ALARMLO     (*(volatile uint32_t *)(TIMG0_BASE + 0x034))
#define T1ALARMHI     (*(volatile uint32_t *)(TIMG0_BASE + 0x038))
#define T1LOADLO      (*(volatile uint32_t *)(TIMG0_BASE + 0x03C)) //Carga de un valor a los 32bits LO
#define T1LOADHI      (*(volatile uint32_t *)(TIMG0_BASE + 0x040)) //Carga de un valor a los 32bits HI
#define T1LOAD        (*(volatile uint32_t *)(TIMG0_BASE + 0x044)) //Cargar valores de LOADLO/HI como reload del contador.

// TIMER de 1MHz para conteo
#define TIMG1_BASE       0x3FF60000
#define TG1_T0CONFIG     (*(volatile uint32_t *)(TIMG1_BASE + 0x000))
#define TG1_T0LO         (*(volatile uint32_t *)(TIMG1_BASE + 0x004))
#define TG1_T0UPDATE     (*(volatile uint32_t *)(TIMG1_BASE + 0x00C))
#define TG1_T0LOADLO     (*(volatile uint32_t *)(TIMG1_BASE + 0x018))
#define TG1_T0LOADHI     (*(volatile uint32_t *)(TIMG1_BASE + 0x01C))
#define TG1_T0LOAD       (*(volatile uint32_t *)(TIMG1_BASE + 0x020))

#define T_EN 				(1UL << 31) //Habilitar timer. Tipo "Play/pause", dependiendo de si esta en HI_LO
#define T_INCREASE			(1UL << 30) //Direccion de conteo del timer. En este caso es en aumento.
#define T_AUTORELOAD		(1UL << 29) //Si el timer se debe "recargar" o reiniciar automaticamente.
#define T_DIVIDER_PRESCALE	(80UL << 13) // 80 MHz/80 = 1 MHz, que da 1us por tick
#define T_LEVEL_INT_EN		(1UL << 11)  // Habilitarr interrupcion de timer.
#define T_ALARM_EN			(1UL << 10)	//Habilitar alarma del timer


//Configuraciones de entrada como entrada con R_PULL_UP
#define FUN_IE			(1UL << 9)
#define MCU_SEL			(1UL << 13)
#define FUN_WPU			(1UL << 8)

#define SET_BIT_HIGH(X)	(1UL << (X)) //Corrimiento de bits para poner un bit a 1

//CTEs de pines
#define SYSTEM_LED	2		//LED Azul de la tarjeta
#define BLACK_BUTTON 22		//Boton negro
#define RED_BUTTON 	23		//Boton rojo

enum{LED_R = 18, LED_G = 19, LED_B = 21}; //Pines del LED RGB.

//CTEs de tiempo
#define DEBOUNCE_US			20000UL		//20 ms de tiempo para antirrebote.
#define BLINK_STANDBY_US	1000000UL	//1s (Para parpadeo de system standby)
#define BLINK_RUNNING_US	250000UL	//Unidad de tiempo de 250ms (Para parpadeo de system running)
#define HOLD_US				3000000UL	//Unidad de tiempo de 3s (Para el contador de 3 segundos de RST)


//Enumeraciones para numeros magicos
typedef enum {NOT_PRESSED, ONE_PRESS, TWO_PRESS, THREE_PRESS, HOLD_PRESS} press_count_e_t; //Para conteo de presiones de boton
typedef enum {ST_OFF, ST_STANDBY, ST_RUNNING} system_state_e_t; //Estados del sistema
typedef enum{LOW,HIGH} bool_voltage_level_e_t; //True/false expresado como niveles de voltaje
typedef enum{ NONE_CAUSE_BTN = -1, BUTTON_BLK_CAUSE = 0, BUTTON_RED_CAUSE = 1} button_id_e_t;	//Para identificar boton causante de interrupcion

typedef struct{
	press_count_e_t button_blk_count;	//Cuenta del boton 1 (GPIO22)
	press_count_e_t button_red_count;	//Cuenta del boton 2 (GPIO23)
	system_state_e_t state; //Indicador de cual es el estado del sistema (OFF,STANDBY,RUNNING)
	volatile bool_voltage_level_e_t led_state; //Bandera del LED2 (LED azul de la tarjeta)
} system_data_t;


//Banderas de estado del sistema
system_data_t system_data = {
	NOT_PRESSED, 
	NOT_PRESSED,
	ST_OFF,
	LOW
};

intr_handle_t gpio_handle = NULL;

volatile bool button_pressed_flag[2]	= {LOW, LOW};	//Estado de cada boton.
volatile uint32_t last_time_us[2]		= {0, 0};	//Ultimo tiempo de cada boton. (Ultimo momento presionado)

volatile bool btn_intr_flag     = false;	//Bandera: hubo una pulsacion pendiente de procesar.
volatile button_id_e_t  triggering_button = NONE_CAUSE_BTN;		//Boton (BUTTON_1/BUTTON_2) que provoco la ultima pulsacion.

volatile bool hold_fired   = false;	//El hold de 3s sucede.
volatile bool hold_pending = false;	//Bandera: hubo un hold de 3 s, pendiente de procesar en el main.

esp_err_t gpio_set_as_input(volatile uint32_t * gpio_io_mux_reg_addr, unsigned long gpio_num){
	GPIO_ENABLE_W1TC = SET_BIT_HIGH(gpio_num);//Configurar gpio como entrada
	*gpio_io_mux_reg_addr = ((FUN_IE)|(MCU_SEL)|(FUN_WPU)); //GPIO_IO_MUX_REG se configura para entrada con pull_up
	return ESP_OK;
}

esp_err_t gpio_set_as_output(unsigned long gpio_num){
	GPIO_ENABLE_W1TS	= SET_BIT_HIGH(gpio_num);//Poner gpio como salida.
	GPIO_OUT_W1TC 		= SET_BIT_HIGH(gpio_num);//Poner salida de ese gpio en LOW.
	return ESP_OK;
}

esp_err_t gpio_set_output_to(unsigned long gpio_num, bool_voltage_level_e_t set_to){
	if(set_to == HIGH)	GPIO_OUT_W1TS	= SET_BIT_HIGH(gpio_num); //Poner el gpio especificado en alto
	if(set_to == LOW)	GPIO_OUT_W1TC	= SET_BIT_HIGH(gpio_num); //Poner el gpio especificado en bajo
	return ESP_OK;
}

//FUNCIONES PARA TIMERS
void timer_3s_start(void){ //Iniciar timer de 3s.
	T1CONFIG &= ~T_EN; //Detener timer.
	TIMG0_INT_CLR = (1UL << 1);
	T1LOADLO = 0;
	T1LOADHI = 0;
	T1LOAD = 1;
	T1CONFIG |= (T_ALARM_EN | T_EN);	//Habilitar alarma
}

void timer_3s_stop(void){ //DETENER TIMER
	T1CONFIG &= ~(T_ALARM_EN | T_EN);
}


//Interrupcion que se genera despues de 3 segundos sostenidos
static void IRAM_ATTR timer01_isr(void *arg){
	TIMG0_INT_CLR = (1UL << 1);
	T1CONFIG &= ~T_EN;

	hold_fired   = true; //Esta pulsacion fue un hold: al soltar NO cuenta como pulsacion.
	hold_pending = true; //Avisar al main.
}
//Lee el reloj libre de 1 MHz. Escribir UPDATE "congela" el valor antes de leerlo.
static uint32_t time_us(void){
	TG1_T0UPDATE = 1; 
	return TG1_T0LO;
}

static void process_button(button_id_e_t button_id, uint32_t gpio_pin_mask, uint32_t current_time, uint32_t gpios_reg_level){
	if((current_time - last_time_us[button_id]) < DEBOUNCE_US) return; //Rebote: se ignora.
	
	bool is_down = ((gpios_reg_level & gpio_pin_mask) == LOW); //Revisar si el boton del ID esta presionado
	if(is_down == button_pressed_flag[button_id]) return; //No hubo cambio real.
	
	last_time_us[button_id] = current_time; //Actualización del tiempo de interrupcion del boton causante
	button_pressed_flag[button_id]  = is_down; //Actualización de la bandera de estado del boton+

	if(is_down){
		//BOTON PRESIONADO: si es el primero que baja, se inicia el conteo de 3 s. Si es otro que baja de nuevo, durante timer3s, no hace nada.
		if(!button_pressed_flag[1 - button_id]){
			hold_fired = false;
			timer_3s_start();
		}
	} else {
		//BOTON SOLTADO: cuenta como pulsacion solo si NO fue un hold de 3 s.
		if(!hold_fired){ //
			/*Le avisamos al main que hubo pulso corto diciendelo que hubo intr y de que btn fue*/
			triggering_button = button_id;
			btn_intr_flag = true;
		}
		//Si ya no hay botones presionados, se cancela el conteo de 3 s.
		if(!button_pressed_flag[1 - button_id]) timer_3s_stop();
	}
}

//Un unico ISR para ambos GPIO
static void IRAM_ATTR gpio_isr(void *arg){
	uint32_t intr_reg_status = GPIO_STATUS;
	GPIO_STATUS_W1TC = intr_reg_status; //Limpiar solo las banderas que se leyeron
	
	uint32_t level_gpio_in_reg = GPIO_IN;  //Se lee DESPUES de limpiar, para tener el nivel mas reciente.
	uint32_t time_us_now = time_us();
	//SI LA INTERRUPCION FUE CAUSADA POR EL BOTON NEGRO, EJECUTAR:
	if(intr_reg_status & SET_BIT_HIGH(BLACK_BUTTON)) 
		process_button(
			BUTTON_BLK_CAUSE, 
			SET_BIT_HIGH(BLACK_BUTTON), 
			time_us_now, 
			level_gpio_in_reg);
	//SI LA INTERRUPCION FUE CAUSADA POR EL BOTON ROJO, EJECUTAR:
	if(intr_reg_status & SET_BIT_HIGH(RED_BUTTON))   
		process_button(
			BUTTON_RED_CAUSE, 
			SET_BIT_HIGH(RED_BUTTON),   
			time_us_now, 
			level_gpio_in_reg);
}

//Parpadeo del LED indicador
static void IRAM_ATTR timer00_isr(void *arg){
	TIMG0_INT_CLR = (1UL << 0); //Limpiar interrupcion
	T0CONFIG |= T_ALARM_EN; //Habilitar alarma

	system_data.led_state = !system_data.led_state; //Toggle al indicador de nivel del led

	if(system_data.led_state){ //
		GPIO_OUT_W1TS = SET_BIT_HIGH(SYSTEM_LED); //Encender LED si el indicador es ON
	} else {
		GPIO_OUT_W1TC = SET_BIT_HIGH(SYSTEM_LED); //Encender LED si el indicador es OFF
	}
}

esp_err_t set_led_blink_speed(bool is_running){
	T0ALARMLO = is_running ? BLINK_RUNNING_US : BLINK_STANDBY_US; //set_led_blink_speed su
	T0LOADLO = 0; 
	T0LOADHI = 0;
	T0LOAD = 1;
	T0CONFIG |= T_ALARM_EN;
	return ESP_OK;
}

esp_err_t timer_blink_start(void){
	T0CONFIG &= ~(T_EN | T_ALARM_EN);
	TIMG0_INT_CLR = (1UL << 0); //limpiar interrupcion de blinking
	T0ALARMLO = BLINK_STANDBY_US; //Esta función se ejecuta en system_start/stop así que el parpadeo por defecto siempre es STANDBY
	T0LOADLO = 0;
	T0LOADHI = 0;
	T0LOAD = 1;
	system_data.led_state = true;
	esp_err_t err = gpio_set_output_to(SYSTEM_LED, system_data.led_state);
	if(err != ESP_OK) return err;
	T0CONFIG |= (T_ALARM_EN | T_EN);
	return ESP_OK;
}

esp_err_t timer_blink_stop(void){
	T0CONFIG &= ~(T_EN | T_ALARM_EN);
	TIMG0_INT_CLR = (1UL << 0); //Limpiar interrupcion de blinking
	system_data.led_state = false;
	return gpio_set_output_to(SYSTEM_LED, system_data.led_state);
}

//Configurar interrupcion de flanco descendente
esp_err_t gpio_neg_edge_interrupt_setup(volatile uint32_t * gpio_pin_reg_addr){
	*gpio_pin_reg_addr &= ~(0x7UL << 7); //Limpiar configuracion previa
	*gpio_pin_reg_addr |= (2UL << 7); //flanco descendente
	*gpio_pin_reg_addr |= (1UL << 15);
	return ESP_OK;
}

//Configurar interrupcion de flanco ascendente
esp_err_t gpio_pos_edge_interrupt_setup(volatile uint32_t * gpio_pin_reg_addr){
	*gpio_pin_reg_addr &= ~(0x7UL << 7); //Limpiar configuracion previa
	*gpio_pin_reg_addr |= (1UL << 7); //flanco ascendente
	*gpio_pin_reg_addr |= (1UL << 15);
	return ESP_OK;
}

//Interrupcion en ambos flancos. Para detectar presionar Y soltar (Realmente solo se necesita para el hold de 3s).
esp_err_t gpio_any_edge_interrupt_setup(volatile uint32_t * gpio_pin_reg_addr){
	*gpio_pin_reg_addr &= ~(0x7UL << 7);
	*gpio_pin_reg_addr |= (3UL << 7); //Ambos flancos
	*gpio_pin_reg_addr |= (1UL << 15);
	return ESP_OK;
}

//Tag de IRAM_ATTR para interrupcion.
esp_err_t gpio_interrupt_register(void){
	esp_err_t err = esp_intr_alloc(
		ETS_GPIO_INTR_SOURCE,
		ESP_INTR_FLAG_IRAM,
		gpio_isr,
		NULL,
		&gpio_handle
		);
	if(err != ESP_OK){
		ESP_LOGE(TAG, "No se pudo registrar la interrupcion de GPIO (%s)", esp_err_to_name(err));
	}
	return err;
}

esp_err_t system_hw_setup(void){
	esp_err_t err;
	//Configurar entradas (Botones)
	err = gpio_set_as_input(IO_MUX_GPIO22_REG_ADDR, BLACK_BUTTON);
	if(err != ESP_OK) return err;
	err = gpio_set_as_input(IO_MUX_GPIO23_REG_ADDR, RED_BUTTON);
	if(err != ESP_OK) return err;
	
	//Configurar salidas (LEDS)
	err = gpio_set_as_output(SYSTEM_LED);
	if(err != ESP_OK) return err;
	
	err = gpio_set_as_output(LED_R);
	if(err != ESP_OK) return err;
	err = gpio_set_as_output(LED_G);
	if(err != ESP_OK) return err;
	err = gpio_set_as_output(LED_B);
	if(err != ESP_OK) return err;
	
	//Configurar interrupciones
	err = gpio_any_edge_interrupt_setup(GPIO_PIN22_REG_ADDR);
	if(err != ESP_OK) return err;
	err = gpio_any_edge_interrupt_setup(GPIO_PIN23_REG_ADDR);
	if(err != ESP_OK) return err;
	
	return gpio_interrupt_register();
}

//FUNCION PARA CONFIGURAR TIMERS
esp_err_t timers_setup(void){
	//Timers del grupo0
	T0CONFIG = T0LOADLO = T0LOADHI = 0; //T0CONFIG es limpiar los bits que existen.
	T0LOAD = 1;
	T0CONFIG |= ((T_DIVIDER_PRESCALE)|(T_INCREASE)|(T_AUTORELOAD)|(T_LEVEL_INT_EN));
	T0ALARMLO = BLINK_STANDBY_US;
	T0ALARMHI = 0;
	
	T1CONFIG = T1LOADLO = T1LOADHI = 0;
	T1LOAD = 1;
	T1ALARMLO = HOLD_US;
	T1ALARMHI = 0;
	T1CONFIG |= ((T_DIVIDER_PRESCALE)|(T_INCREASE)|(T_LEVEL_INT_EN));
	
	//Reloj libre con frec de 1 MHz para contar el tiempo en general
	//Timer grupo 1 Timer0
	TG1_T0CONFIG = TG1_T0LOADLO = TG1_T0LOADHI = 0;
	TG1_T0LOAD = 1;
	TG1_T0CONFIG = ((T_DIVIDER_PRESCALE)|(T_INCREASE)|(T_EN));
	
	TIMG0_INT_CLR = ((1UL << 0)|(1UL << 1));
	TIMG0_INT_ENA |= ((1UL << 0)|(1UL<<1));
	
	esp_err_t err = esp_intr_alloc(ETS_TG0_T0_LEVEL_INTR_SOURCE, ESP_INTR_FLAG_IRAM, timer00_isr, NULL, NULL);
	if(err != ESP_OK){
		ESP_LOGE(TAG, "No se pudo registrar la interrupcion de Timer0 (%s)", esp_err_to_name(err));
		return err;
	}

	err = esp_intr_alloc(ETS_TG0_T1_LEVEL_INTR_SOURCE, ESP_INTR_FLAG_IRAM, timer01_isr, NULL, NULL);
	if(err != ESP_OK){
		ESP_LOGE(TAG, "No se pudo registrar la interrupcion de Timer1 (%s)", esp_err_to_name(err));
		return err;
	}

	return ESP_OK;
}

//FUNCION PARA ENCEDER/APAGAR pines RGB A
esp_err_t rgb_set(bool r, bool g, bool b){
	esp_err_t err;
	err = gpio_set_output_to(LED_R, r ? HIGH : LOW);
	if(err != ESP_OK) return err;
	err = gpio_set_output_to(LED_G, g ? HIGH : LOW);
	if(err != ESP_OK) return err;
	return gpio_set_output_to(LED_B, b ? HIGH : LOW);
}

//FUNCION PARA LLAMAR rgb_set E IMPRIMIR LO QUE SUCEDE
esp_err_t rgb_set_and_print(bool r, bool g, bool b, const char *color_name){
	esp_err_t err = rgb_set(r, g, b);
	if(err != ESP_OK) return err;
	ESP_LOGI(TAG, "LED RGB: %s (R=%d G=%d B=%d)", color_name, r, g, b);
	return ESP_OK;
}

//MENSAJE DE BIENVENIDA
esp_err_t welcome_message(void){
	ESP_LOGI(TAG, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
	ESP_LOGI(TAG, "LABORATORIO 1");
	ESP_LOGI(TAG, "Materia: Software Embebido");
	ESP_LOGI(TAG, "Alumno: Armando Garcia ");
	ESP_LOGI(TAG, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
	ESP_LOGI(TAG, ".....................INSTRUCCIONES......................");
	ESP_LOGI(TAG, "A)	Mantenga presionado un boton por 3s para INICIAR.");
	ESP_LOGI(TAG, "B1)	Boton Negro	x1	=	LED RGB SIN CAMBIOS");
	ESP_LOGI(TAG, "B2)	Boton Negro	x2	=	LED RGB CYAN");
	ESP_LOGI(TAG, "B3)	Boton Rojo	x1  =	LED RGB MAGENTA");
	ESP_LOGI(TAG, "B4)	Boton Rojo	x2	=	LED RGB AMARILLO");
	ESP_LOGI(TAG, "B5)	Cualquier	x3	=	Reiniciar cuentas");
	ESP_LOGI(TAG, "C)	Cualquier HOLD	=	Durante running apaga el sistema.");
	ESP_LOGI(TAG, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
	ESP_LOGI(TAG, "Esperando inicio... (mantenga un boton 3 s)");
	return ESP_OK;
}


//INCIAR SISTEMA 
esp_err_t system_start(void){
	//A pesar de que el sistema y la estructura de datos se encuentra inicializada correctamente en button_n_count = NOT_PRESSED.
	//Esta es una función que se manda llamar para el reinicio del systema posteriormente.
	system_data.button_blk_count = NOT_PRESSED; //Condicion de inicio del black button
	system_data.button_red_count = NOT_PRESSED; //Condicion de inicio del red button
	
	esp_err_t err = rgb_set(LOW, LOW, LOW); //Apagar RGB
	if(err != ESP_OK) return err; //Revision de error por defecto
	
	
	err = timer_blink_start(); //Iniciar parpadeo
	if(err != ESP_OK) return err;
	
	system_data.state = ST_STANDBY; //El sistema se encontraba apagado y ahora se inicia (standby)
	ESP_LOGW(TAG, "Sistema iniciado... Estado: Standby"); //LOGW solo para hacerlo amarillo.
	return ESP_OK;
}

esp_err_t system_shutdown(void){
	ESP_LOGW(TAG, "Apagando Sistema..."); //LOGW solo para hacerlo amarillo.
	
	esp_err_t err = timer_blink_stop(); //Detener parpadeo
	if(err != ESP_OK) return err;
	
	err = rgb_set(LOW, LOW, LOW); //Apagar RGB.
	if(err != ESP_OK) return err;
	
	//Volvemos al estado de arranque.
	system_data.button_blk_count = NOT_PRESSED;
	system_data.button_red_count = NOT_PRESSED;
	system_data.state = ST_OFF;

	return welcome_message(); //Imprimimos el mensaje de bienvenida de nuevo
}

esp_err_t counters_reset(void){ //Reset del contador
	ESP_LOGW(TAG, "Reiniciando Cuentas"); //Se hace warning simplemente para que sea amarillo
	
	//Volvemos al estado de standby, solo reseteamos cuentas y ponemos sistema en standby
	system_data.button_blk_count = NOT_PRESSED;
	system_data.button_red_count = NOT_PRESSED;

	esp_err_t err = rgb_set(LOW, LOW, LOW);  //Led RGB apagado
	if(err != ESP_OK) return err;

	err = set_led_blink_speed(false);
	if(err != ESP_OK) return err;

	system_data.state = ST_STANDBY; //El standby se pone al final para primero ejecutar el resto del codigo (realmente poner en standby)
	ESP_LOGI(TAG, "Estado: Standby");
	return ESP_OK;
}

//Funcion que se llama en main cuando hay interrupcion a atender
esp_err_t button_pressed(button_id_e_t button_id){
	if(system_data.state == ST_OFF) return ESP_OK; //Sistema apagado: no es un error, pero no se debe contar, asi que se ignora.
	
	press_count_e_t *button_count = (button_id == BUTTON_BLK_CAUSE) ? &system_data.button_blk_count //Apuntar cuenta a la cuenta de uno de los botones.
	                                                : &system_data.button_red_count;
	*button_count = (press_count_e_t)(*button_count + 1); //Aumentar la cuenta a la que se apunta, osease, aumentar cuenta del boton causante

	//Tercera pulsacion de cualquiera de los dos botones: reinicio de cuentas.
	if(*button_count >= THREE_PRESS){
		return counters_reset(); //Salida de funcion cuando sucede 3er pulsacion
	}

	esp_err_t err = ESP_OK;
	if(system_data.state != ST_RUNNING){ //Iniciar el sistema tras un standby
		system_data.state = ST_RUNNING;
		err = set_led_blink_speed(true); //Se actualiza la velocidad de parpadeo. Se usa un solo timer para ambas vel. parpadeo
		if(err != ESP_OK) return err;
	}
	ESP_LOGI(TAG, "Estado: Running");

	//Las cuentas no estan relacionadas entre si; el ultimo color pedido reemplaza al anterior.
	if(button_id == BUTTON_BLK_CAUSE){ //Causado por el boton negro.
		if(*button_count == TWO_PRESS) 
			err = rgb_set_and_print(
					LOW, HIGH, HIGH, 
					"CYAN");
	} else {//Sino es negro entonces es causado por el boton rojo.
		if(*button_count== ONE_PRESS) 
			err = rgb_set_and_print(
				HIGH, LOW, HIGH, 
				"MAGENTA");
		else if(*button_count == TWO_PRESS) 
			err = rgb_set_and_print(
					HIGH, HIGH, LOW, 
					"AMARILLO");
	}

	return err;
}

void app_main(void)
{
	//ESP_ERROR_CHECK: Las fallas imprimen error y detienen ejecucion.
	ESP_ERROR_CHECK(timers_setup());
	ESP_ERROR_CHECK(system_hw_setup());
	ESP_ERROR_CHECK(welcome_message());
	while (true) {
		if(hold_pending){ //
			hold_pending = false;
			//Si el sistema esta apagado, arrancar systema
			//para cualquier otra cosa está STBY o RUNNING, entonces el usuario está apagandop
			esp_err_t err = (system_data.state == ST_OFF) ? system_start() : system_shutdown(); 
			if(err != ESP_OK){ //Imprimir el error si sucede algo
				ESP_LOGE(TAG, "Error al cambiar el estado del sistema (%s)", esp_err_to_name(err));
			}
		}

		if(btn_intr_flag){ //Si hubo una interrupcion por boton se enciende una bandera y esta parte del codigo la atiende
			esp_err_t err = button_pressed(triggering_button); //Revisar si aumenta cuenta o pone el sistema en running.
			if(err != ESP_OK){
				ESP_LOGE(TAG, "Error al procesar la pulsacion (%s)", esp_err_to_name(err));
			}
			btn_intr_flag = false;
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}