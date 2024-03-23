#include "logger.h"

static char *TAG = "main";

void test_remote_logging(){
    while(1){
        ESP_LOGI(TAG, "Remote Logging Test...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
void app_main(void)
{
    static httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    /* Start the remote logger for the first time */
    server = start_remote_logger();

    test_remote_logging();
}
