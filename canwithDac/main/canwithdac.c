//CHANGE RXBYTES TO 34 35 29 for hardware
// 34 37 17 for excel sheet
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include "driver/gpio.h"
#include "driver/can.h"
#include <driver/dac.h>

#define CAN_TXD_PIN 21
#define CAN_RXD_PIN 22
#define SLOW 0x32
#define MEDIUM 0x64
#define FAST 0x96


uint8_t mode = 0;

static const can_timing_config_t t_config = CAN_TIMING_CONFIG_250KBITS();
static const can_filter_config_t f_config = CAN_FILTER_CONFIG_ACCEPT_ALL();
static const can_general_config_t g_config = CAN_GENERAL_CONFIG_DEFAULT(CAN_TXD_PIN, CAN_RXD_PIN, CAN_MODE_NORMAL);



void can_init()
{   
    if (can_driver_install(&g_config, &t_config, &f_config) == ESP_OK){
        printf("%s\n","CAN installed");
    } 
    else {
        printf("%s\n","CAN not installed");
        return;
    }

    //Start CAN driver
    if (can_start() == ESP_OK) {
        printf("%s\n","CAN started");
    } 
    else {
        printf("%s\n","Failed to start CAN");
        return;
    }
    printf("%s\n","CAN initalised");
}

static void can_task(void * pvParameters)
{
    can_init();
    dac_output_enable(DAC_CHANNEL_1);   
    while(1)
    {
        can_message_t rx_msg;
        can_receive(&rx_msg, portMAX_DELAY);
        if (rx_msg.identifier == 0xAAAA)
        {
            mode = rx_msg.data[5];
            if(mode == SLOW)
            {
                dac_output_voltage(DAC_CHANNEL_1,0);
            }
            else if(mode == MEDIUM)
            {
                dac_output_voltage(DAC_CHANNEL_1,128);
            }    
            else if(mode == FAST)
            {
                dac_output_voltage(DAC_CHANNEL_1,255);
            }
        }
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void app_main()
{
    
    xTaskCreate(can_task,"can_tx_task",2048,NULL,5,NULL);
}
