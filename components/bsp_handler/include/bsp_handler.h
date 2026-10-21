#ifndef BSP_HANDLER_H
#define BSP_HANDLER_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp_err.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "soc/interrupts.h"

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

//Enumeraciones para numeros magicos
typedef enum {ST_OFF, ST_STANDBY, ST_RUNNING} system_state_e_t; //Estados del sistema
typedef enum{ NONE_CAUSE_BTN = -1, BUTTON_BLK_CAUSE = 0, BUTTON_RED_CAUSE = 1} button_id_e_t;	//Para identificar boton causante de interrupcion

typedef enum {NOT_PRESSED, ONE_PRESS, TWO_PRESS, THREE_PRESS, HOLD_PRESS} press_count_e_t; //Para conteo de presiones de boton
typedef enum{LOW,HIGH} bool_voltage_level_e_t; //True/false expresado como niveles de voltaje

typedef struct{
	press_count_e_t button_blk_count;	//Cuenta del boton 1 (GPIO22)
	press_count_e_t button_red_count;	//Cuenta del boton 2 (GPIO23)
	system_state_e_t state; //Indicador de cual es el estado del sistema (OFF,STANDBY,RUNNING)
	volatile bool_voltage_level_e_t led_state; //Bandera del LED2 (LED azul de la tarjeta)
} system_data_t;

//Banderas de estado del sistema
extern system_data_t system_data;

esp_err_t gpio_set_as_input(volatile uint32_t *, unsigned long);
esp_err_t gpio_set_as_output(unsigned long);
esp_err_t gpio_set_output_to(unsigned long, bool_voltage_level_e_t);

#include "bsp_timers.h"

#endif