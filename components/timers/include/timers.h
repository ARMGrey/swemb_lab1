#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "soc/interrupts.h"

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

//CTEs de tiempo
#define DEBOUNCE_US			20000UL		//20 ms de tiempo para antirrebote.
#define BLINK_STANDBY_US	1000000UL	//1s (Para parpadeo de system standby)
#define BLINK_RUNNING_US	250000UL	//Unidad de tiempo de 250ms (Para parpadeo de system running)
#define HOLD_US				3000000UL	//Unidad de tiempo de 3s (Para el contador de 3 segundos de RST)

extern volatile bool hold_fired;//El hold de 3s sucede.
extern volatile bool hold_pending;//Bandera: hubo un hold de 3 s, pendiente de procesar en el main.

extern const char *TAG;

void timer_3s_start(void);
void timer_3s_stop(void);
uint32_t time_us(void);
esp_err_t set_led_blink_speed(bool);
esp_err_t timer_blink_start(void);
esp_err_t timer_blink_stop(void);
esp_err_t timers_setup(intr_handler_t, intr_handler_t);