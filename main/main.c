/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "esp_wireguard.h"
#include "sync_time.h"
//#include "http_server_s.h"
#include "relay_handler.h"
#include "rom/ets_sys.h"
#include "dht11_handler.h"
#include "esp_sntp.h"


//static const char *TAG = "flower_keeper";
static wireguard_config_t wg_config = ESP_WIREGUARD_CONFIG_DEFAULT();

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

#if !CONFIG_IDF_TARGET_LINUX
#include <esp_wifi.h>
#include <esp_system.h>
#include "nvs_flash.h"
#include "esp_eth.h"
#endif  // !CONFIG_IDF_TARGET_LINUX

const char* led_tag = "leds";
static const char *TAG = "flower_keeper";
uint8_t led1 = GPIO_NUM_17;
uint8_t led2 = GPIO_NUM_16;


#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN  (64)

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

// Function to create the JSON response
static char* create_json_response(struct dht11_reading reading)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    cJSON_AddNumberToObject(root, "status", reading.status);
    cJSON_AddNumberToObject(root, "temperature", reading.temperature);
    cJSON_AddNumberToObject(root, "humidity", reading.humidity);

    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return json_str;
}


/* An HTTP GET handler */
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN], dec_param[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN] = {0};
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
                example_uri_decode(dec_param, param, strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
                ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
                example_uri_decode(dec_param, param, strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
                ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
                example_uri_decode(dec_param, param, strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
                ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = "Hello World!" ; //(const char*) req->user_ctx;
    //httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);


    struct dht11_reading reading;
    if (xQueuePeek(dht11_queue, &reading, 0) == pdTRUE)
    {
        char *json_response = create_json_response(reading);
        if (json_response != NULL)
        {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, json_response, strlen(json_response));
            free(json_response);
        }
        else
        {
            httpd_resp_send_500(req);
        }
    }
    else
    {
        httpd_resp_send_500(req);
    }


    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL  // "Hello World!"
};

/* An HTTP POST handler */
static esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t echo = {
    .uri       = "/echo",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};




static void handle_leds(int led_num_value, int state_value)
{
    if (led_num_value == 1)
    {
        gpio_set_level(led1, state_value);
        ESP_LOGI(led_tag, "Setting led %d in state %d", led_num_value, state_value);
    }
    else if (led_num_value == 2)
    {
        gpio_set_level(led2, state_value);
        ESP_LOGI(led_tag, "Setting led %d in state %d", led_num_value, state_value);
    }
    else
    {
        ESP_LOGI(led_tag, "No such led!");
    }
}

/*
We expect that there is a json in post body with such content:
{
"led_num": 1, // int
"state": 0 // int or bool (may be 1 or 0)
}

curl -X POST http://192.168.0.100/leds -H 'Content-Type: application/json' -d '{"led_num":2,"state":1}'

*/
static esp_err_t leds_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret = 0;
    int remaining = req->content_len;

    // Read the data from the request
    while (remaining > 0) {
        // This will copy the request body into the buffer
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                // Retry receiving if timeout occurred
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }

    // Null-terminate the buffer
    buf[ret] = '\0';
    ESP_LOGI(TAG, "Received: %s", buf);

    // Parse the JSON data
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    // Extract the "led_num" value
    cJSON *led_num = cJSON_GetObjectItem(json, "led_num");
    if (!cJSON_IsNumber(led_num)) {
        ESP_LOGE(TAG, "Invalid led_num");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // Extract the "state" value
    cJSON *state = cJSON_GetObjectItem(json, "state");
    if (!cJSON_IsNumber(state)) {
        ESP_LOGE(TAG, "Invalid state");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // Get the values
    int led_num_value = led_num->valueint;
    int state_value = state->valueint;

    ESP_LOGI(TAG, "LED Number: %d, State: %d", led_num_value, state_value);

    // Work with the values - for now just here
    // in future - using queue
    handle_leds(led_num_value, state_value);

    // Clean up
    cJSON_Delete(json);

    // Send a response
    const char resp[] = "Post request processed successfully";
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}


static const httpd_uri_t leds = {
    .uri       = "/leds",
    .method    = HTTP_POST,
    .handler   = leds_post_handler,
    .user_ctx  = NULL
};



static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &echo);
        httpd_register_uri_handler(server, &leds);
        #if CONFIG_EXAMPLE_BASIC_AUTH
        httpd_register_basic_auth(server);
        #endif
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

#if !CONFIG_IDF_TARGET_LINUX
static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
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
#endif // !CONFIG_IDF_TARGET_LINUX








void app_main(void)
{
    // Create the queue to hold relay timing configurations
    gpio_toggle_queue = xQueueCreate(1, sizeof(gpio_toggle_config_t));
    dht11_queue = xQueueCreate(1, sizeof(struct dht11_reading));
    // init dht11
    //DHT11_init(GPIO_NUM_4);
    // Create the DHT11 reading task
    //xTaskCreatePinnedToCore(&dht11_task, "dht11_task", 2048, NULL, tskIDLE_PRIORITY, NULL, 1);

    /*if (dht11_queue == NULL)
    {
        ESP_LOGI(TAG, "Failed to create dht11 queue\n");
    }*/

    // Create the relay toggle task
    xTaskCreatePinnedToCore(&gpio_toggle_task, "gpio_toggle_task", 2048, NULL, tskIDLE_PRIORITY, NULL, 1);

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
    //ESP_LOGI(TAG, "1");
    gpio_set_direction(led2, GPIO_MODE_OUTPUT);
    gpio_set_level(led1, 1);
    gpio_set_level(led2, 1);  // they are reversed

        /**/
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI


    // very important time sync with sntp  ???
    // https://github.com/trombik/esp_wireguard/issues/29#issuecomment-1331375949
    // https://docs.espressif.com/projects/esp-idf/en/v4.4.3/esp32/api-reference/system/system_time.html#sntp-time-synchronization
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();


    // NECESSARITY OF USING SNTP SYNC IS VERY UNCLEAR
    // I REALLY HATE THIS LIB


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
    //ets_delay_us(1);

    /* Start the server for the first time */
    server = start_webserver();

    while (server) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
