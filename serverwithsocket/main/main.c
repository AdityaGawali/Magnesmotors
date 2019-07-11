#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <driver/dac.h>

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

//#define DACPIN CONFIG_DAC

const static char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
// embedded binary data
extern const uint8_t webpage_html_start[] asm("_binary_webpage_html_start");
extern const uint8_t webpage_html_end[]   asm("_binary_webpage_html_end");


//WebSocket frame receive queue
QueueHandle_t WebSocket_rx_queue;
static const char *TAG = "socket with server";


static EventGroupHandle_t wifi_event_group;

const int IPV4_GOTIP_BIT = BIT0;
const int IPV6_GOTIP_BIT = BIT1;
const int WIFI_CONNECTED_BIT = BIT0;


void task_process_WebSocket( void *pvParameters ){
    (void)pvParameters;

    //frame buffer
    WebSocket_frame_t __RX_frame;
    char* data_buf;

    //create WebSocket RX Queue
    WebSocket_rx_queue = xQueueCreate(10,sizeof(WebSocket_frame_t));

    while (1){
        //receive next WebSocket frame from queue
        if(xQueueReceive(WebSocket_rx_queue,&__RX_frame, 3*portTICK_PERIOD_MS)==pdTRUE){

            //write frame inforamtion to UART
            printf("New Websocket frame. Length %d, payload %.*s \r\n", __RX_frame.payload_length, __RX_frame.payload_length, __RX_frame.payload);
            data_buf = __RX_frame.payload;
            if(strcmp(data_buf,"slow") == 0)
            {
            	dac_output_voltage(DAC_CHANNEL_1, 0);
            }
            else if(strcmp(data_buf,"medium") == 0)
            {
                dac_output_voltage(DAC_CHANNEL_1, 128);
            }
            else if(strcmp(data_buf,"fast") == 0)
            {
                dac_output_voltage(DAC_CHANNEL_1, 255);
            }


            //loop back frame
            WS_write_data(__RX_frame.payload, __RX_frame.payload_length);

            //free memory
            if (__RX_frame.payload != NULL)
                free(__RX_frame.payload);

        }
    }
}
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        /* enable ipv6 */
        tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, IPV4_GOTIP_BIT);
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, IPV4_GOTIP_BIT);
        xEventGroupClearBits(wifi_event_group, IPV6_GOTIP_BIT);
        break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
        xEventGroupSetBits(wifi_event_group, IPV6_GOTIP_BIT);
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP6");

        char *ip6 = ip6addr_ntoa(&event->event_info.got_ip6.ip6_info.ip);
        ESP_LOGI(TAG, "IPv6: %s", ip6);
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
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}


void app_main(void)
{

    esp_log_level_set("wifi", ESP_LOG_NONE);

    nvs_flash_init();
    initialise_wifi();
    dac_output_enable(DAC_CHANNEL_1);
    
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
    

    // start the HTTP Server task
    xTaskCreate(&http_server, "http_server", 2048, NULL, 5, NULL);
    xTaskCreate(&task_process_WebSocket, "ws_process_rx", 2048, NULL, 5, NULL);

    //Create Websocket Server Task
    xTaskCreate(&ws_server, "ws_server", 2048, NULL, 5, NULL);

}
