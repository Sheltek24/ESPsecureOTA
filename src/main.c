#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "led_strip.h"

#include "esp_crt_bundle.h"

#define blinkPin 0
#define updatePin 1

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void WiFiInit();
void updateOTA();
void initRGB();
void setColorRGB(uint8_t r, uint8_t g, uint8_t b);
esp_err_t _http_event_handler(esp_http_client_event_t *evt);

led_strip_handle_t led_strip;
#define setRGBcolorDisconnect setColorRGB(20, 20, 0)
#define setRGBcolorConnect setColorRGB(0, 0, 20)
#define setRGBcolorOTAdone setColorRGB(0, 20, 0)
#define setRGBcolorOTAerror setColorRGB(20, 0, 0)

uint32_t blinkTime = 0;
bool blinkFlag = 0;

bool updateButtonFlag = 0;
uint32_t updateButtonTime = 0;
uint32_t WiFiDelayTime = 0;
bool IPisOkFlag = 0;
#define LOCATION_BUF_SIZE 1024
static char location_buf[LOCATION_BUF_SIZE];
static bool got_location = false;

void app_main()
{
    gpio_reset_pin(blinkPin);
    gpio_set_direction(blinkPin, GPIO_MODE_OUTPUT);
    gpio_set_direction(updatePin, GPIO_MODE_INPUT);
    gpio_pullup_en(updatePin);

    initRGB();
    WiFiInit();

    while (1)
    {
        if (!blinkFlag && esp_log_timestamp() - blinkTime > 80)
        {
            gpio_set_level(blinkPin, 1);
            blinkTime = esp_log_timestamp();
            blinkFlag = 1;
        }
        else if (blinkFlag && esp_log_timestamp() - blinkTime > 80)
        {
            gpio_set_level(blinkPin, 0);
            blinkTime = esp_log_timestamp();
            blinkFlag = 0;
        }

        updateOTA();
        vTaskDelay(1);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI("wifi", "Connected to AP, waiting for IP...");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI("wifi", "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        setRGBcolorConnect;
        IPisOkFlag = 1;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED && esp_log_timestamp() - WiFiDelayTime > 500)
    {
        WiFiDelayTime = esp_log_timestamp();
        ESP_LOGW("wifi", "Disconnected, reconnecting...");
        setRGBcolorDisconnect;
        IPisOkFlag = 0;
        esp_wifi_connect();
    }
}

void WiFiInit()
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Redmi 10 2022",
            .password = "Shel102022",
        },
    };
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    esp_wifi_start();
    esp_wifi_connect();
}

void updateOTA()
{
    if (!updateButtonFlag && !gpio_get_level(updatePin) && esp_log_timestamp() - updateButtonTime > 5000)
    {
        updateButtonFlag = 1;
        esp_http_client_config_t http_cfg = {
            // .url = "https://drive.google.com/uc?export=download&id=1bGXt2xLd75f74No2ifbY8skEfJoN0dy7",
            // .url = "https://drive.google.com/uc?export=download&id=1-TYUwrtR7Y90hffn3OJ4tKkK33SvqQ2y",
            .url = "https://github.com/Sheltek24/ESPsecureOTA/releases/latest/download/firmware.bin",
            .event_handler = _http_event_handler,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .timeout_ms = 90000,
            .buffer_size = 8192,
            .buffer_size_tx = 8192,
            .max_redirection_count = 5,
        };
        esp_https_ota_config_t ota_cfg = {
            .http_config = &http_cfg,
        };

        // esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
        // esp_err_t err = esp_http_client_perform(client);
        // if (err == ESP_OK)
        // {
        //     ESP_LOGI("HTTP", "Status = %d, content_length = %d", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));
        //     setRGBcolorOTAdone;
        //     vTaskDelay(pdMS_TO_TICKS(1000));
        //     led_strip_clear(led_strip);
        //     if (IPisOkFlag)
        //         setRGBcolorConnect;
        //     else
        //         setRGBcolorDisconnect;
        // }
        // else
        // {
        //     ESP_LOGE("HTTP", "HTTP failed: %s", esp_err_to_name(err));
        //     setRGBcolorOTAerror;
        //     vTaskDelay(pdMS_TO_TICKS(1000));
        //     led_strip_clear(led_strip);
        //     if (IPisOkFlag)
        //         setRGBcolorConnect;
        //     else
        //         setRGBcolorDisconnect;
        // }
        // esp_http_client_cleanup(client);

        esp_err_t ret = esp_https_ota(&ota_cfg);
        if (ret == ESP_OK)
        {
            ESP_LOGI("OTA", "OTA OK, restarting...");
            setRGBcolorOTAdone;
            vTaskDelay(pdMS_TO_TICKS(1000));
            led_strip_clear(led_strip);
            esp_restart();
        }
        else
        {
            ESP_LOGE("OTA", "OTA failed: %s", esp_err_to_name(ret));
            setRGBcolorOTAerror;
            vTaskDelay(pdMS_TO_TICKS(1000));
            led_strip_clear(led_strip);
            if (IPisOkFlag)
                setRGBcolorConnect;
            else
                setRGBcolorDisconnect;
        }
    }
    else if (updateButtonFlag && gpio_get_level(updatePin))
    {
        updateButtonFlag = 0;
        updateButtonTime = esp_log_timestamp();
    }
}

void initRGB()
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = 8,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

void setColorRGB(uint8_t r, uint8_t g, uint8_t b)

{
    led_strip_set_pixel(led_strip, 0, g, r, b);
    led_strip_refresh(led_strip);
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI("HTTP", "Connected");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI("HTTP", "ON_HEADER len=%d: %.*s", evt->data_len, evt->data_len, (char *)evt->data);
        if (evt->data_len > 9)
        {
            if ((evt->data_len >= 9) && (strncasecmp((const char *)evt->data, "Location:", 9) == 0))
            {
                int copy_len = evt->data_len - 9;
                const char *p = (const char *)evt->data + 9;
                while (copy_len > 0 && (*p == ' ' || *p == '\t'))
                {
                    p++;
                    copy_len--;
                }
                if (copy_len > 0)
                {
                    int to_copy = (copy_len < LOCATION_BUF_SIZE - 1) ? copy_len : LOCATION_BUF_SIZE - 1;
                    memcpy(location_buf, p, to_copy);
                    location_buf[to_copy] = '\0';
                    got_location = true;
                    ESP_LOGI("HTTP", "Saved Location header: %s", location_buf);
                }
            }
        }
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI("HTTP", "ON_REDIRECT len=%d: %.*s", evt->data_len, evt->data_len, (char *)evt->data);
        {
            int to_copy = evt->data_len;
            if (to_copy > LOCATION_BUF_SIZE - 1)
                to_copy = LOCATION_BUF_SIZE - 1;
            memcpy(location_buf, evt->data, to_copy);
            location_buf[to_copy] = '\0';
            got_location = true;
            ESP_LOGI("HTTP", "Saved redirect Location: %s", location_buf);
        }
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI("HTTP", "Data chunk len=%d; free heap=%d", evt->data_len, esp_get_free_heap_size());
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI("HTTP", "Finished; free heap=%d", esp_get_free_heap_size());
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGE("HTTP", "Event error");
        break;
    default:
        break;
    }
    return ESP_OK;
}