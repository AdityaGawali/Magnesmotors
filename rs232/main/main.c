#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include "driver/gpio.h"

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN 18
#define RXD_PIN 19

#define HARDWARE 0
#define CELL 1
#define BASIC 2
uint8_t state = 0;


char basic_data[] = {0xDD,0xA5,0x03,0x00,0xFF,0xFD,0x77};
char cell_data[] = {0xDD,0xA5,0x04,0x00,0xFF,0xFC,0x77};
char hard_data[] = {0xDD,0xA5,0x05,0x00,0xFF,0xFB,0x77};
float total_voltage = 0.0;
float current = 0.0;
float temp_raw = 0.0;
uint16_t cell_voltage[14];


uint16_t compute_checksum(const uint8_t* data)
{
    uint16_t checksum = 0;
    checksum = data[2] + data[3];
    for(int i = 0;i <data[3];i++)
    {
        checksum = checksum + data[4+i];
    }
    checksum = ~(checksum) + 1;
    return checksum;    
}



void init() 
{
    const uart_config_t uart_config = 
    {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

int sendData(const char* data)
{
    const int len = 7;
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    return txBytes;
}



static void rx_task()
{
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    int txbytes = 0;
    while (1) 
    {   
        if(state == HARDWARE)
        {
            txbytes = sendData(hard_data);
        }
        else if(state == CELL)
        {
            txbytes = sendData(cell_data);
        }
        else if(state == BASIC)
        {
           txbytes =  sendData(basic_data);
        }
        printf("%d bytes %d state %s\n",txbytes,state,"Data transmitted");
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        int rxbytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_RATE_MS);
        printf("%d\n",rxbytes);
        if(rxbytes == 34)
        {
            total_voltage = ((float)(((data[4]<<8) | data[5])*10)/1000);
            current = ((float)(((data[6]<<8) | data[7])*10)/1000);

            temp_raw = data[30]*0.2;
            printf("%f V\n",total_voltage);
            printf("%f C\n", temp_raw );
            printf("%f A\n", current );
            state = CELL;
        }
        else if(rxbytes == 35)
        {   
            for(int i =0;i<14;i++)
            {
                cell_voltage[i] = (data[2*i + 4]<<8) | data[2*i + 5];
            }
            for(int i =0;i<14;i++)
            {
                printf("%d cell voltage: %d mV\n",i,cell_voltage[i]);
            }
            state = BASIC;
        }
        else if(rxbytes == 29)
        {   

            printf("%s\t","Hardware version: " );
            for(int i = 0;i<data[3];i++)
            {
                printf("%c",data[4+i]);

            }
            printf("\n");
   

            state = BASIC;
        }
        else
        {
            printf("%s\n","ERROR");
            for(int i = 0 ; i < rxbytes;i++)
            {
                printf("%d\n",data[i] );
            }
            
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);

    }

    
    free(data);
}

void app_main()
{
    init();
    xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
}
