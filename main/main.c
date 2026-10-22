#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "bsp_handler.h"

intr_handle_t gpio_handle = NULL;

volatile bool button_pressed_flag[2]	= {LOW, LOW};	//Estado de cada boton.
volatile uint32_t last_time_us[2]		= {0, 0};	//Ultimo tiempo de cada boton. (Ultimo momento presionado)

volatile bool btn_intr_flag     = false;	//Bandera: hubo una pulsacion pendiente de procesar.
volatile button_id_e_t  triggering_button = NONE_CAUSE_BTN;		//Boton (BUTTON_1/BUTTON_2) que provoco la ultima pulsacion.
volatile button_id_e_t	timer_trig_btn = NONE_CAUSE_BTN;

static void process_red_btn(uint32_t current_time, uint32_t level_gpio_input_reg, bool red_is_pressed, bool blk_is_pressed){
	if(current_time - last_time_us[BUTTON_RED_CAUSE] < DEBOUNCE_US) return;
	if(red_is_pressed){
		if(timer_trig_btn == NONE_CAUSE_BTN){
			timer_trig_btn = BUTTON_RED_CAUSE;
			timer_3s_start();
		}
	}
	else{
		if (hold_fired){
			hold_fired = false;
			return;
		}
		if(!hold_pending){
			triggering_button = BUTTON_RED_CAUSE;
			btn_intr_flag = true;
		}
		if(!blk_is_pressed){ 
			timer_trig_btn = NONE_CAUSE_BTN;
			timer_3s_stop();
		}
	}

}

static void process_blk_btn(uint32_t current_time, uint32_t level_gpio_input_reg, bool red_is_pressed, bool blk_is_pressed){
	if(current_time - last_time_us[BUTTON_BLK_CAUSE] < DEBOUNCE_US) return;
	if(blk_is_pressed){
		if(timer_trig_btn == NONE_CAUSE_BTN){
			timer_trig_btn = BUTTON_BLK_CAUSE;
			timer_3s_start();
		}
	}
	else{
		if (hold_fired){ //Ignorar el primer soltado de boton tras un hold
			hold_fired = false;
			return;
		}
		if(!hold_pending){
			triggering_button = BUTTON_BLK_CAUSE;
			btn_intr_flag = true;
		}
		if(!red_is_pressed){
			timer_trig_btn = NONE_CAUSE_BTN;
			 timer_3s_stop();
		}

	}
}

//Un unico ISR para ambos GPIO
static void IRAM_ATTR gpio_isr(void *arg){
	uint32_t intr_reg_status = GPIO_STATUS;
	GPIO_STATUS_W1TC = intr_reg_status; //Limpiar solo las banderas que se leyeron
	
	uint32_t level_gpio_input_reg = GPIO_IN;  //Se lee DESPUES de limpiar, para tener el nivel mas reciente.
	uint32_t time_us_now = time_us();

	bool red_pressed_status = (((level_gpio_input_reg & (SET_BIT_HIGH(RED_BUTTON))))== LOW);
	bool blk_pressed_status = (((level_gpio_input_reg & (SET_BIT_HIGH(BLACK_BUTTON))))== LOW);	
	//SI LA INTERRUPCION FUE CAUSADA POR EL BOTON NEGRO, EJECUTAR:
	if(intr_reg_status & SET_BIT_HIGH(BLACK_BUTTON)) 
		process_blk_btn(
			time_us_now, 
			level_gpio_input_reg,
			red_pressed_status,
			blk_pressed_status
		);
	//SI LA INTERRUPCION FUE CAUSADA POR EL BOTON ROJO, EJECUTAR:
	if(intr_reg_status & SET_BIT_HIGH(RED_BUTTON))   
		process_red_btn(
			time_us_now, 
			level_gpio_input_reg,
			red_pressed_status,
			blk_pressed_status
		);
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

//Interrupcion que se genera despues de 3 segundos sostenidos
static void IRAM_ATTR timer01_isr(void *arg){
	TIMG0_INT_CLR = (1UL << 1);
	T1CONFIG &= ~T_EN;

	hold_fired   = true; //Esta pulsacion fue un hold: al soltar NO cuenta como pulsacion.
	hold_pending = true; //Avisar al main.
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

void app_main(void)
{
	//ESP_ERROR_CHECK: Las fallas imprimen error y detienen ejecucion.
	ESP_ERROR_CHECK(timers_setup(timer00_isr, timer01_isr));
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