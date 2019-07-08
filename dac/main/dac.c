
#include <driver/dac.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//Saturates around 225
//for 10V -> 203

static void dac_task(void * pvParameters)
{
    
    dac_output_enable(DAC_CHANNEL_1);   
    while(1)
    {   
        for(int i = 150; i<256;i++)
        {
        dac_output_voltage(DAC_CHANNEL_1,i);
        printf("%d\n",i);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }
}

void app_main()
{
    
    xTaskCreate(dac_task,"dac_tx_task",2048,NULL,5,NULL);
}
