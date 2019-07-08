//CHANGE RXBYTES TO 34 35 29 for hardware
// 34 37 17 for excel sheet
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"


#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "driver/gpio.h"

#include "../../components/WebsocketTask/WebSocket_Task.h"


#include "driver/uart.h"
#include "soc/uart_struct.h"
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
    float current;
    float temp;
    uint16_t cell_voltage[14];
    uint8_t chrg_mode;
}bms_data_t;



const static char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
extern const uint8_t webpage_html_start[] asm("_binary_webpage_html_start");
extern const uint8_t webpage_html_end[]   asm("_binary_webpage_html_end");

uint8_t* mac_address = NULL;

//WebSocket frame receive queue
QueueHandle_t WebSocket_rx_queue;
static const char *TAG = "socket with server";


static EventGroupHandle_t wifi_event_group;

const int IPV4_GOTIP_BIT = BIT0;
const int IPV6_GOTIP_BIT = BIT1;
const int WIFI_CONNECTED_BIT = BIT0;


static const can_timing_config_t t_config = CAN_TIMING_CONFIG_250KBITS();
static const can_filter_config_t f_config = CAN_FILTER_CONFIG_ACCEPT_ALL();
static const can_general_config_t g_config = CAN_GENERAL_CONFIG_DEFAULT(CAN_TXD_PIN, CAN_RXD_PIN, CAN_MODE_NORMAL);

static const int RX_BUF_SIZE = 1024;

const char basic_data[] = {0xDD,0xA5,0x03,0x00,0xFF,0xFD,0x77};
const char cell_data[] = {0xDD,0xA5,0x04,0x00,0xFF,0xFC,0x77};
const char hard_data[] = {0xDD,0xA5,0x05,0x00,0xFF,0xFB,0x77};





char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
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
        printf("%s\n","UART");
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

        TickType_t xStart;
        TickType_t xDelay = 2000 / portTICK_PERIOD_MS;
        xStart = xTaskGetTickCount();  
        while(((xTaskGetTickCount - xStart)/portTICK_PERIOD_MS) < xDelay);
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

        }
        else if(rxbytes == 35)
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
            printf("%s\n",".");
        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);
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
        message.data_length_code = 6;
        for(int i = 0;i<6;i++)
        {
            message.data[i] = 0;
        }
        message.data[0] =  8>>BMS->raw_total_voltage;; //HIGH BYTE
        message.data[1] = message.data[1] | BMS->raw_total_voltage; //LOW BYTE
        message.data[2] =  8>>BMS->raw_total_current; //HIGH BYTE
        message.data[3] = message.data[3] | BMS->raw_total_current;//LOW BYTE
        message.data[4] = BMS->raw_temp;
        message.data[5] = BMS->chrg_mode;
        if (can_transmit(&message, portMAX_DELAY) == ESP_OK) 
        {
            printf("%s\n","Message queued for CAN transmission");
        }
        else 
        {
            printf("%s\n","Failed to queue message for CANN transmission");
       
        }
    vTaskDelay(3000/portTICK_PERIOD_MS);
    }
}


void task_process_WebSocket(bms_data_t* BMS)
{
    

    //frame buffer
    WebSocket_frame_t __RX_frame;
    char* data_buf;

    //create WebSocket RX Queue
    WebSocket_rx_queue = xQueueCreate(10,sizeof(WebSocket_frame_t));

    char* total_voltage_str = (char*)malloc(12);
    char* current_str = (char*)malloc(12);
    char* temp_str = (char*)malloc(12);
    char* chargingMode_str = (char*)malloc(12);


    while (1){
        //receive next WebSocket frame from queue
        printf("%s\n","SOCKET");
        if(xQueueReceive(WebSocket_rx_queue,&__RX_frame, 3*portTICK_PERIOD_MS)==pdTRUE)
        {

            //write frame inforamtion to UART
            printf("New Websocket frame. Length %d, payload %.*s \r\n", __RX_frame.payload_length, __RX_frame.payload_length, __RX_frame.payload);
            
            if(strcmp("slow", __RX_frame.payload)==0)
            {
                BMS->chrg_mode = 0x32;
            }
            else if(strcmp("medium", __RX_frame.payload)==0)
            {
                BMS->chrg_mode = 0x64;
            }
            else if(strcmp("fast", __RX_frame.payload)==0)
            {
                BMS->chrg_mode = 0x96;
            }
            gcvt(BMS->total_voltage,6,total_voltage_str);
            gcvt(BMS->current,6,current_str);
            gcvt(BMS->temp,6,temp_str);

            total_voltage_str = concat("voltage ",total_voltage_str);
            current_str = concat("current ",current_str);
            temp_str = concat("temp ",temp_str);

            WS_write_data(total_voltage_str,strlen(total_voltage_str));
            WS_write_data(current_str,strlen(current_str));
            WS_write_data(temp_str,strlen(temp_str));

            if (__RX_frame.payload != NULL)
                free(__RX_frame.payload);

        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);

    }
}
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        /* enable ipv6 */
        tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, IPV4_GOTIP_BIT);
        
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, IPV4_GOTIP_BIT);
        xEventGroupClearBits(wifi_event_group, IPV6_GOTIP_BIT);
        break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
        xEventGroupSetBits(wifi_event_group, IPV6_GOTIP_BIT);
        

        char *ip6 = ip6addr_ntoa(&event->event_info.got_ip6.ip6_info.ip);
        
    default:
        break;
    }
    return ESP_OK;
}

static void http_server_netconn_serve(struct netconn *conn) 
{

    struct netbuf *inbuf;
    char *buf;
    u16_t buflen;
    err_t err;

    err = netconn_recv(conn, &inbuf);

    if (err == ERR_OK) 
    {
      
        netbuf_data(inbuf, (void**)&buf, &buflen);
        
        // extract the first line, with the request
        char *first_line = strtok(buf, "\n");
        
        if(first_line) 
        {
            
            // default page
            if(strstr(first_line, "GET / ")) 
            {
                netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
                netconn_write(conn, webpage_html_start, webpage_html_end - webpage_html_start, NETCONN_NOCOPY);

            }
        }
        else printf("Unkown request\n");
    }
    
    // close the connection and free the buffer
    netconn_close(conn);
    netbuf_delete(inbuf);
}

static void http_server(void *pvParameters) {
    
    struct netconn *conn, *newconn;
    err_t err;
    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, NULL, 80);
    netconn_listen(conn);
    printf("HTTP Server listening...\n");
    do {
        err = netconn_accept(conn, &newconn);
        printf("New client connected\n");
        if (err == ERR_OK) {
            http_server_netconn_serve(newconn);
            netconn_delete(newconn);
        }
        vTaskDelay(1); //allows task to be pre-empted
    } while(err == ERR_OK);
    netconn_close(conn);
    netconn_delete(conn);
    printf("\n");
}

static void initialise_wifi(void)
{   
    mac_address = (uint8_t*)malloc(6 * sizeof(uint8_t));
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "SSID",
            .password = "PASSWORD",
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    esp_wifi_get_mac(ESP_IF_WIFI_STA,mac_address);
}


void app_main(void)
{

    bms_data_t* BMS;
     

    esp_log_level_set("wifi", ESP_LOG_NONE);

    nvs_flash_init();
    initialise_wifi();
    uart_init();
    can_init();
    
    // wait for connection
    printf("Waiting for connection to the wifi network...\n ");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    printf("Connected\n\n");
    
    // print the local IP address
    tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
    printf("IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
    printf("Subnet mask: %s\n", ip4addr_ntoa(&ip_info.netmask));
    printf("Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw)); 
    printf("macAddress: \t"); 
    for(int i = 0;i<6;i++)
    {   
        
        printf("%X",mac_address[i]);

    }
    printf("\n");

    // start the HTTP Server task
    xTaskCreatePinnedToCore(&http_server, "http_server", 2048, NULL, 5, NULL,0);
    //Create Websocket Server Task
    xTaskCreatePinnedToCore(&ws_server, "ws_server", 2048, NULL, 5, NULL,0);
    

    xTaskCreatePinnedToCore(&task_process_WebSocket, "ws_process_rx", 2048, &BMS, 5, NULL,1);
    xTaskCreatePinnedToCore(rx_task, "uart_rx_task", 2048, &BMS, 5, NULL,1);
    xTaskCreatePinnedToCore(can_task,"can_tx_task",2048,&BMS,5,NULL,1);




}
