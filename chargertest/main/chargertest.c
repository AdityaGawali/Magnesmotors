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
#include "esp_task_wdt.h"

#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>


#include "WebSocket_Task.h"

#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "driver/gpio.h"
#include <driver/dac.h>


#include "driver/spi_master.h"
#include "esp_spi_flash.h"
#include "mdns.h"

#include "http_server.h"
#include "wifi_manager.h"


const static char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
extern const uint8_t webpage_html_start[] asm("_binary_webpage_html_start");
extern const uint8_t webpage_html_end[]   asm("_binary_webpage_html_end");


//WebSocket frame receive queue
QueueHandle_t WebSocket_rx_queue;


static EventGroupHandle_t wifi_event_group;

const int IPV4_GOTIP_BIT = BIT0;
const int IPV6_GOTIP_BIT = BIT1;
const int WIFI_CONNECTED_BIT = BIT0;

void task_process_WebSocket()
{
    

    //frame buffer
    WebSocket_frame_t __RX_frame;

    //create WebSocket RX Queue
    WebSocket_rx_queue = xQueueCreate(10,sizeof(WebSocket_frame_t));

    while (1){
        //receive next WebSocket frame from queue
        if(xQueueReceive(WebSocket_rx_queue,&__RX_frame, 3*portTICK_PERIOD_MS)==pdTRUE)
        {

            //write frame inforamtion to UART
            printf("New Websocket frame. Length %d, payload %.*s \r\n", __RX_frame.payload_length, __RX_frame.payload_length, __RX_frame.payload);
            
            if(strcmp("slow", __RX_frame.payload)==0)
            {
                dac_output_voltage(DAC_CHANNEL_1,0);
            }
            else if(strcmp("medium", __RX_frame.payload)==0)
            {
                dac_output_voltage(DAC_CHANNEL_1,97);
            }
            else if(strcmp("fast", __RX_frame.payload)==0)
            {

                dac_output_voltage(DAC_CHANNEL_1,200);

            }

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
        

        
    default:
        break;
    }
    return ESP_OK;
}

static void http_server_netconn_serve_sta(struct netconn *conn) 
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

static void http_server_sta(void *pvParameters) {
    
    struct netconn *conn, *newconn;
    err_t err;
    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, NULL, 81);
    netconn_listen(conn);
    printf("HTTP Server listening...\n");
    do {
        err = netconn_accept(conn, &newconn);
        printf("New client connected\n");
        if (err == ERR_OK) {
            http_server_netconn_serve_sta(newconn);
            netconn_delete(newconn);
        }
        vTaskDelay(1); //allows task to be pre-empted
    } while(err == ERR_OK);
    netconn_close(conn);
    netconn_delete(conn);
    printf("\n");
}


void cb_connection_ok(void *pvParameter)
{
	tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
    printf("IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
    printf("Subnet mask: %s\n", ip4addr_ntoa(&ip_info.netmask));
    printf("Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw)); 
    xTaskCreatePinnedToCore(&http_server_sta, "http_server_sta", 2048, NULL, 5, NULL,0);
    //Create Websocket Server Task
    xTaskCreatePinnedToCore(&ws_server, "ws_server", 2048, NULL, 5, NULL,0);
    

    xTaskCreatePinnedToCore(&task_process_WebSocket, "ws_process_rx", 2048,NULL, 5, NULL,1);

   

}

void app_main(void)
{


    esp_log_level_set("wifi", ESP_LOG_NONE);
    dac_output_enable(DAC_CHANNEL_1);   

    wifi_manager_start();
    wifi_manager_set_callback(EVENT_STA_GOT_IP, &cb_connection_ok);





}
