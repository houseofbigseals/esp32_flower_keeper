/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <cJSON.h>
#include "esp_wireguard.h"
#include "sync_time.h"
#include "http_server_s.h"


#if !CONFIG_IDF_TARGET_LINUX
#include <esp_wifi.h>
#include <esp_system.h>
#include "nvs_flash.h"
#include "esp_eth.h"
#endif  // !CONFIG_IDF_TARGET_LINUX



//static const char *TAG = "flower_keeper";
static wireguard_config_t wg_config = ESP_WIREGUARD_CONFIG_DEFAULT();

/*
static esp_err_t wireguard_setup(wireguard_ctx_t* ctx)
{
    esp_err_t err = ESP_FAIL;

    ESP_LOGI(TAG, "Initializing WireGuard.");
    wg_config.private_key = CONFIG_WG_PRIVATE_KEY;
    wg_config.listen_port = CONFIG_WG_LOCAL_PORT;
    wg_config.public_key = CONFIG_WG_PEER_PUBLIC_KEY;
    if (strcmp(CONFIG_WG_PRESHARED_KEY, "") != 0) {
        wg_config.preshared_key = CONFIG_WG_PRESHARED_KEY;
    } else {
        wg_config.preshared_key = NULL;
    }
    wg_config.allowed_ip = CONFIG_WG_LOCAL_IP_ADDRESS;
    wg_config.allowed_ip_mask = CONFIG_WG_LOCAL_IP_NETMASK;
    wg_config.endpoint = CONFIG_WG_PEER_ADDRESS;
    wg_config.port = CONFIG_WG_PEER_PORT;
    wg_config.persistent_keepalive = CONFIG_WG_PERSISTENT_KEEP_ALIVE;

    err = esp_wireguard_init(&wg_config, ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wireguard_init: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Connecting to the peer.");
    err = esp_wireguard_connect(ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wireguard_connect: %s", esp_err_to_name(err));
        return err;
    }

    err = ESP_OK;
    return err;
}
*/


void app_main(void)
{


    esp_err_t err;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    wireguard_ctx_t ctx = {0};

    static httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */

    vTaskDelay( pdMS_TO_TICKS(10) );
    ESP_LOGI(TAG, "1");
    gpio_set_direction(led1, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "1");
    gpio_set_direction(led2, GPIO_MODE_OUTPUT);
    gpio_set_level(led1, 1);
    gpio_set_level(led2, 1);  // they are reversed

        /**/
#if !CONFIG_IDF_TARGET_LINUX
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET
#endif // !CONFIG_IDF_TARGET_LINUX


    //setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    //tzset();
    //localtime_r(&now, &timeinfo);
    //strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    //ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);

    //ESP_ERROR_CHECK(wireguard_setup(&ctx));

    ESP_LOGI(TAG, "Initializing WireGuard.");
    wg_config.private_key = CONFIG_WG_PRIVATE_KEY;
    wg_config.listen_port = CONFIG_WG_LOCAL_PORT;
    wg_config.public_key = CONFIG_WG_PEER_PUBLIC_KEY;
    if (strcmp(CONFIG_WG_PRESHARED_KEY, "") != 0) {
        wg_config.preshared_key = CONFIG_WG_PRESHARED_KEY;
    } else {
        wg_config.preshared_key = NULL;
    }
    wg_config.allowed_ip = CONFIG_WG_LOCAL_IP_ADDRESS;
    wg_config.allowed_ip_mask = CONFIG_WG_LOCAL_IP_NETMASK;
    wg_config.endpoint = CONFIG_WG_PEER_ADDRESS;
    wg_config.port = CONFIG_WG_PEER_PORT;
    wg_config.persistent_keepalive = CONFIG_WG_PERSISTENT_KEEP_ALIVE;

    ESP_ERROR_CHECK(esp_wireguard_init(&wg_config, &ctx));

    ESP_LOGI(TAG, "Connecting to the peer.");

    // https://github.com/trombik/esp_wireguard/issues/33
    /*
    TODO:
    So if we activate CONFIG_ESP_NETIF_BRIDGE_EN or CONFIG_LWIP_PPP_SUPPORT in idf.py menuconfig, it will fix the problem.

    For a test, I tried activating "Enable PPP support (new/experimental)" (Name: LWIP_PPP_SUPPORT), and it works.

    So in short, activate CONFIG_LWIP_PPP_SUPPORT in your project configuration in ESP-IDF-v5, recompile your project, and the wireguard tunnel will work as intended.
    
    */
    ESP_ERROR_CHECK(esp_wireguard_connect(&ctx));

    /*
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wireguard_setup: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "Halting due to error");
        while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }*/

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        err = esp_wireguardif_peer_is_up(&ctx);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Peer is up");
            break;
        } else {
            ESP_LOGI(TAG, "Peer is down");
        }
    }

    //ESP_ERROR_CHECK(esp_wireguard_connect(&ctx));
    /*
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wireguard_connect: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "Halting due to error");
        while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    */
    ESP_LOGI(TAG, "Peer is up");
    esp_wireguard_set_default(&ctx);


    /* Start the server for the first time */
    server = start_webserver();

    while (server) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
