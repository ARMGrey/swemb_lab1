#include <stdio.h>
#include "timers.h"

volatile bool hold_fired   = false;	//El hold de 3s sucede.
volatile bool hold_pending = false;	//Bandera: hubo un hold de 3 s, pendiente de procesar en el main.

const char *TAG = "SYS_MSG";

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

//Lee el reloj libre de 1 MHz. Escribir UPDATE "congela" el valor antes de leerlo.
uint32_t time_us(void){
	TG1_T0UPDATE = 1; 
	return TG1_T0LO;
}

esp_err_t set_led_blink_speed(bool is_running){
	T0ALARMLO = is_running ? BLINK_RUNNING_US : BLINK_STANDBY_US; //set_led_blink_speed su
	T0LOADLO = 0; 
	T0LOADHI = 0;
	T0LOAD = 1;
	T0CONFIG |= T_ALARM_EN;
	return ESP_OK;
}

//FUNCION PARA CONFIGURAR TIMERS
esp_err_t timers_setup(intr_handler_t timer00ISR, intr_handler_t timer01ISR){
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
	
	esp_err_t err = esp_intr_alloc(ETS_TG0_T0_LEVEL_INTR_SOURCE, ESP_INTR_FLAG_IRAM, timer00ISR, NULL, NULL);
	if(err != ESP_OK){
		ESP_LOGE(TAG, "No se pudo registrar la interrupcion de Timer0 (%s)", esp_err_to_name(err));
		return err;
	}

	err = esp_intr_alloc(ETS_TG0_T1_LEVEL_INTR_SOURCE, ESP_INTR_FLAG_IRAM, timer01ISR, NULL, NULL);
	if(err != ESP_OK){
		ESP_LOGE(TAG, "No se pudo registrar la interrupcion de Timer1 (%s)", esp_err_to_name(err));
		return err;
	}

	return ESP_OK;
}
