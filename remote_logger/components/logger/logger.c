#include <stdio.h>
#include <stdarg.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "protocol_examples_common.h"

#include <esp_http_server.h>


#include "logger.h"

#define QUEUE_LENGTH 20
#define BUFFER_SIZE 256

static char *TAG = "remote_logger";

static QueueHandle_t queue;

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void *arg)
{
    ///char * data = network_outgoing_buffer;
    char *data = "async works!";
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

static esp_err_t async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) {
        return ESP_ERR_NO_MEM;
    }
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
    if (ret != ESP_OK) {
        free(resp_arg);
    }
    return ret;
}

static int __vprintf(const char *format, va_list args){
    //Note: calling any ESP_LOGx() will cause recursion in this function

    char *network_outgoing_buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    if (network_outgoing_buffer == NULL) {
        return 0;
    }

    //char num_char = snprintf(network_outgoing_buffer, BUFFER_SIZE, format, args);
    char num_char = vsnprintf(network_outgoing_buffer, BUFFER_SIZE, format, args);
    if (num_char > 0){
        xQueueSend(queue, &network_outgoing_buffer, portMAX_DELAY);
    }
    else{
        //vsnprintf failed, free buffer
        free(network_outgoing_buffer);
    }

    return num_char; 

}


/*
 * This handler notifies when client is connected. 
 */
static esp_err_t controller_handler(httpd_req_t *req)
{

    if (req->method == HTTP_GET) {

        ESP_LOGI(TAG, "Handshake done,  new connection opened");
        return ESP_OK;
    }
    
    uint8_t *buf = NULL;
    httpd_ws_frame_t ws_pkt;
    esp_err_t ret = ESP_OK;
    char *network_outgoing_buffer = "";
    //char *network_outgoing_buffer1 = "ESP_LOGx()";

    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    //set max_len = 0 to get the frame len
    ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }
    if (ws_pkt.len) {

        //ws_pkt.len + 1 is for NULL terminated string
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;

        //Set max_len = ws_pkt.len to get the frame payload
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            free(buf);
            return ret;
        }
    }

    if ((strcmp((char*)ws_pkt.payload,"log") == 0)){  

        // When a connected client sends "log" command, redirect logging to websocket
        ESP_LOGI(TAG, "Redirecting Logging To WebSocket");
        esp_log_set_vprintf(&__vprintf);
        ESP_LOGI(TAG, "Success!");

        while(1){
            if (xQueueReceive(queue, &network_outgoing_buffer, portMAX_DELAY) == pdTRUE) {  

                //send logs syncronously
                ws_pkt.payload = (uint8_t *)network_outgoing_buffer;
                ws_pkt.len = strlen(network_outgoing_buffer);
                ret = httpd_ws_send_frame(req, &ws_pkt);
                
                //send logs asyncronously
                //ret = async_send(req->handle, req);

                if(ret != ESP_OK){ 
                    //when client disconnects or problem with remote logging, redirect logging to UART0
                    esp_log_set_vprintf(&vprintf);
                    ESP_LOGW(TAG, "Client disconnected. Logging redirected to UART0");
                    free(network_outgoing_buffer);
                    return !ESP_OK;
                }

                free(network_outgoing_buffer);
            }
        }
    }

    free(buf);
    return ESP_OK;
}

static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = controller_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};


static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

httpd_handle_t start_webserver(void)
{
    
    httpd_handle_t server = NULL;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /**
     * the params used to configure confi is defined in esp_http_server.h
     * config.stack_size is set to 4096
     * Sometimes the webserver raises a stackoverflow error running at this stack_size. 
    */
    config.stack_size = 4096;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ws);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    // Redirect the output stream to UART0 when Wi-Fi disconnects
    esp_log_set_vprintf(&vprintf);

    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

httpd_handle_t start_remote_logger(){
    httpd_handle_t server = NULL;

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET

    queue = xQueueCreate(QUEUE_LENGTH, sizeof(int));
    
    // Create tasks for functionA and functionB
    //xTaskCreate(ws_log_task, "log_task", configMINIMAL_STACK_SIZE + 128, NULL, tskIDLE_PRIORITY + 1, NULL);
    
    return start_webserver();

    /* Redirect the output stream to UART0
     * esp_log_set_vprintf(&vprintf);
     * This function returns a handler to the previous/original log handler
     * prototype: vprintf_like_t esp_log_set_vprintf(vprintf_like_t func);
     */
    

}