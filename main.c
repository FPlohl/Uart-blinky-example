#include <stdbool.h>
#include <stdint.h>

#include "app_uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "bsp.h"
#include "nordic_common.h"
#include "nrf_drv_clock.h"
#include "sdk_errors.h"
#include "app_error.h"

#include <stdio.h>
#include "boards.h"
#include <string.h>
#include "nrf_uarte.h"
#include "nrf.h"

#define TASK_DELAY        200           /**< Task delay. Delays a LED0 task for 200 ms */
#define TIMER_PERIOD      1000          /**< Timer period. LED1 timer will expire after 1000 ms */

TaskHandle_t  led2_toggle_task_handle;   /**< Reference to LED2 toggling FreeRTOS task. */

TimerHandle_t led1_toggle_timer_handle;  /**< Reference to LED1 toggling FreeRTOS timer. */

TaskHandle_t  pwm_task_handle;   /**< Reference for pwm FreeRTOS task. */

#define MAX_TEST_DATA_BYTES     (15U)                /**< max number of test bytes to be used for tx and rx. */
#define UART_TX_BUF_SIZE 256                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 256                         /**< UART RX buffer size. */

uint8_t rx_buff[100];
uint8_t i;
volatile int pwm = 1;

void uart_event_handle(app_uart_evt_t * p_event)
{
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
    else if(p_event->evt_type == APP_UART_DATA_READY){ // character in buffer and available to read
        uint8_t cr;

        app_uart_get(&cr);  // read character from rx buffer
        app_uart_put(cr);   // echo read character

        rx_buff[i++] = cr;

        if(strstr(rx_buff, "high")){
            bsp_board_led_on(0);
            
            uint8_t status[] = "\r\nLED ON\r\n";

            for (int i = 0; i < strlen(status); i++){
                app_uart_put(status[i]);
            }
            memset(rx_buff,0,100);
            i = 0;
        }

        else if(strstr(rx_buff, "low")){
            bsp_board_led_off(0);
            
            uint8_t status[] = "\r\nLED OFF\r\n";

            for (int i = 0; i < strlen(status); i++){
                app_uart_put(status[i]);
            }
            memset(rx_buff,0,100);
            i = 0;
        }
    }
}

static void pwm_function (void * pvParameter){
    UNUSED_PARAMETER(pvParameter);
    while (true)
    {
        bsp_board_led_on(BSP_BOARD_LED_3);
        vTaskDelay(pwm);
        bsp_board_led_off(BSP_BOARD_LED_3);
        vTaskDelay(10-pwm);
    }
}

static void led2_toggle_task_function (void * pvParameter)
{
    UNUSED_PARAMETER(pvParameter);
    while (true)
    {
        bsp_board_led_invert(BSP_BOARD_LED_2);
        vTaskDelay(TASK_DELAY+100);
    }
}

static void led_toggle_timer_callback (void * pvParameter)
{
    UNUSED_PARAMETER(pvParameter);
    bsp_board_led_invert(BSP_BOARD_LED_1);
}

#define UART_HWFC APP_UART_FLOW_CONTROL_DISABLED

int main(void)
{
    ret_code_t err_code;
    bsp_board_init(BSP_INIT_LEDS);
    /* Initialize clock driver for better time accuracy in FREERTOS */
    err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);

    const app_uart_comm_params_t comm_params =
        {
            RX_PIN_NUMBER,
            TX_PIN_NUMBER,
            RTS_PIN_NUMBER,
            CTS_PIN_NUMBER,
            UART_HWFC,
            false,
            NRF_UARTE_BAUDRATE_115200
        };
    
    APP_UART_FIFO_INIT(&comm_params,
                         UART_RX_BUF_SIZE,
                         UART_TX_BUF_SIZE,
                         uart_event_handle,
                         APP_IRQ_PRIORITY_LOWEST,
                         err_code);

    /* Create task for LED0 and LED 2 blinking with priority set to 2 */
    UNUSED_VARIABLE(xTaskCreate(led2_toggle_task_function, "LED2", configMINIMAL_STACK_SIZE + 100, NULL, 1, &led2_toggle_task_handle));
    UNUSED_VARIABLE(xTaskCreate(pwm_function, "PWM", configMINIMAL_STACK_SIZE + 200, NULL, 2, &pwm_task_handle));

    /* Start timer for LED1 blinking */
    led1_toggle_timer_handle = xTimerCreate( "LED1", TIMER_PERIOD, pdTRUE, NULL, led_toggle_timer_callback);
    UNUSED_VARIABLE(xTimerStart(led1_toggle_timer_handle, 0));

    /* Activate deep sleep mode */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    APP_ERROR_CHECK(err_code);
    /* Start FreeRTOS scheduler. */
    vTaskStartScheduler();

    while (true)
    {
        /* FreeRTOS should not be here... FreeRTOS goes back to the start of stack
         * in vTaskStartScheduler function. */
    }
}

/**
 *@}
 **/
