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

typedef struct 
{
    float total_voltage;
    uint16_t  raw_total_voltage;
    float current;
    uint16_t  raw_total_current;
    float temp;
    uint8_t raw_temp;
    uint16_t cell_voltage[14];
}bms_data_t;


static const can_timing_config_t t_config = CAN_TIMING_CONFIG_250KBITS();
static const can_filter_config_t f_config = CAN_FILTER_CONFIG_ACCEPT_ALL();
static const can_general_config_t g_config = CAN_GENERAL_CONFIG_DEFAULT(CAN_TXD_PIN, CAN_RXD_PIN, CAN_MODE_NORMAL);

static const int RX_BUF_SIZE = 1024;

const char basic_data[] = {0xDD,0xA5,0x03,0x00,0xFF,0xFD,0x77};
const char cell_data[] = {0xDD,0xA5,0x04,0x00,0xFF,0xFC,0x77};
const char hard_data[] = {0xDD,0xA5,0x05,0x00,0xFF,0xFB,0x77};

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



static void rx_task(bms_data_t* BMS)
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
        //vTaskDelay(2000 / portTICK_PERIOD_MS);

        int rxbytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_RATE_MS);
        
        if(rxbytes == 34)
        {
            BMS->total_voltage = ((float)(((data[4]<<8) | data[5])*10)/1000);
            BMS->current = ((float)(((data[6]<<8) | data[7])*10)/1000);
            BMS->temp = data[30]*0.2;
            
            printf("%f V\n",BMS->total_voltage);
            printf("%f C\n", BMS->temp );
            printf("%f A\n", BMS->current );
            state = CELL;
        }
        else if(rxbytes == 37)
        {   
            for(int i =0;i<14;i++)
            {
                BMS->cell_voltage[i] = (data[2*i + 4]<<8) | data[2*i + 5];
            }
            for(int i =0;i<14;i++)
            {
                printf("%d cell voltage: %d mV\n",i,BMS->cell_voltage[i]);
            }
            state = BASIC;
        }
        else if(rxbytes == 17)
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
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    free(data);
}

static void can_task(bms_data_t* BMS)
{   

     while(1)
    {
        printf("%s\n","CAN");
        BMS->raw_total_voltage = (BMS->total_voltage*100);
        BMS->raw_total_current = (BMS->current*100);
        BMS->raw_temp = (BMS->temp/0.2);

        can_message_t message;
        message.identifier = 0xAAAA;
        message.flags = CAN_MSG_FLAG_EXTD;
        message.data_length_code = 5;
        for(int i = 0;i<5;i++)
        {
            message.data[i] = 0;
        }
        message.data[0] =  8>>BMS->raw_total_voltage;; //HIGH BYTE
        message.data[1] = message.data[1] | BMS->raw_total_voltage; //LOW BYTE
        message.data[2] =  8>>BMS->raw_total_current; //HIGH BYTE
        message.data[3] = message.data[3] | BMS->raw_total_current;//LOW BYTE
        message.data[4] = BMS->raw_temp;

        if (can_transmit(&message, portMAX_DELAY) == ESP_OK) 
        {
            printf("%s\n","Message queued for CAN transmission");
        }
        else 
        {
            printf("%s\n","Failed to queue message for CANN transmission");
       
        }
    vTaskDelay(2000/portTICK_PERIOD_MS);
    }
}

void app_main()
{
    bms_data_t* BMS;
    uart_init();
    can_init();
    xTaskCreate(rx_task, "uart_rx_task", 2048,&BMS, 6, NULL);
    xTaskCreate(can_task,"can_tx_task",2048,&BMS,5,NULL);
}
