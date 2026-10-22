#include <stdio.h>
#include "bsp_handler.h"

system_data_t system_data = {
	NOT_PRESSED,
	NOT_PRESSED,
	ST_OFF,
	LOW
};

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