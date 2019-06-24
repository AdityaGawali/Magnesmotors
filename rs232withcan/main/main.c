#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include "driver/gpio.h"
#include "driver/can.h"



#define RS232_TXD_PIN 18
#define RS232_RXD_PIN 19
#define CAN_TXD_PIN 21
#define CAN_RXD_PIN 22




typedef enum state
{
    HARDWARE= 0,
    CELL,
    BASIC
}State;

static const can_timing_config_t t_config = CAN_TIMING_CONFIG_250KBITS();
static const can_filter_config_t f_config = CAN_FILTER_CONFIG_ACCEPT_ALL();
static const can_general_config_t g_config = CAN_GENERAL_CONFIG_DEFAULT(CAN_TXD_PIN, CAN_RXD_PIN, CAN_MODE_NORMAL);

static const int RX_BUF_SIZE = 1024;

const char basic_data[] = {0xDD,0xA5,0x03,0x00,0xFF,0xFD,0x77};
const char cell_data[] = {0xDD,0xA5,0x04,0x00,0xFF,0xFC,0x77};
const char hard_data[] = {0xDD,0xA5,0x05,0x00,0xFF,0xFB,0x77};


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

void uart_init() 
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
    uart_set_pin(UART_NUM_1, RS232_TXD_PIN, RS232_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}


void can_init()
{   
    if (can_driver_install(&g_config, &t_config, &f_config) == ESP_OK) 
    {
        printf("%s\n","driver installed");
    } 
    else 
    {
        printf("%s\n","Driver not installed");
        return;
    }

    //Start CAN driver
    if (can_start() == ESP_OK) 
    {
        printf("%s\n","Driver started");
    } 
    else 
    {
        printf("%s\n","Failed to start driver");
        return;
    }
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
    State state = HARDWARE;
    
    while (1) 
    {   
        if(state == HARDWARE)
        {
            sendData(hard_data);
        }
        else if(state == BASIC)
        {
            sendData(basic_data);
        }
        else if(state == CELL)
        {
            sendData(cell_data);
        }

        printf("%d %s\n",state,"RS232 Data transmitted");
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        int rxbytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_RATE_MS);
        
        if(rxbytes == 34)
        {
            total_voltage = ((float)(((data[4]<<8) | data[5])*10)/1000);
            current = ((float)(((data[6]<<8) | data[7])*10)/1000);
            
            temp_raw = data[30]*0.2;
            printf("%f V\n",total_voltage);
            printf("%f C\n", temp_raw );
            printf("%f A\n", current );
    
            can_message_t message;
            message.identifier = 0xAAAA;
            message.flags = CAN_MSG_FLAG_EXTD;
            message.data_length_code = 2;
            message.data[0] =  0xAA; //HIGH BYTE
            message.data[1] = 0xAA; //LOW BYTE
            if (can_transmit(&message, portMAX_DELAY) == ESP_OK) 
            {
                printf("%s\n","Message queued for CAN transmission");
            }
            else 
            {
                printf("%s\n","Failed to queue message for CANN transmission");
           
            }
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
            printf("%s\n","RS232 data not received");
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    free(data);
}

void app_main()
{
    uart_init();
    can_init();
    xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
}
