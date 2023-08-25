#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include "esp_eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <time.h>
#include "uart.h"

#include <stdlib.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_client.h"
#include "freertos/event_groups.h"
#include "driver/spi_master.h"


#define EXAMPLE_ESP_WIFI_SSID      "softAP_SE"
#define EXAMPLE_ESP_WIFI_PASS      "SE_12345678"
#define EXAMPLE_MAX_STA_CONN       10

#define CHIP_SELECT 15  //BLANCO
#define SCLK_PIN    2   //VERDE
#define MISO_PIN    4   //AMARILLO
#define MOSI_PIN    5   //AZUL

#define MS5611_CMD_RESET 0x1E
#define MS5611_CMD_ADC_READ 0x00
#define MS5611_CMD_TEMP_CONV 0x58 //TEMPERATURA OSR = 4096
#define MS5611_CMD_PRESS_CONV 0x48 //PRESION OSR = 4096

spi_device_handle_t spi2;
spi_transaction_t trans;
uint16_t coeficientes[8] = {0};
uint8_t cmd;

static const char *TAG = "softAP_WebServer";
char cad[20];
float TEMP_FINAL=0;
char tempascii[20];

static void delayMs(uint32_t ms)
{
    vTaskDelay(ms/portTICK_PERIOD_MS);
}


void myItoa(uint16_t number, char* str, uint8_t base) //TERMINADA
{
    uint16_t i = 0;  
    while(number)
    {
        uint16_t r = number % base;
        if (r >= 0 && r <=9) {
            str[i] = '0' + r;
        }
        else {
            str[i] = 'A' + (r - 10);
        }
        i++;
        number = number / base;
    }
    str[i] = '\0';
    reverse(str, 0, i - 1);
}
/* --- TOOLS --- */

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
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

/* FUNCIONES Y COMPLEMENTOS DE SENSADO */

void sensor(int sensores[])
{
    if(uartKbhit(UART2_PORT))
    {
        int len = uart_read_bytes(UART2_PORT, cadToRec, (READ_BUF_SIZE - 1), 0);
        if (len)
        {
            token = strtok(cadToRec, ":");
            for(int i=1; i<3; i++)
            {
                sensores[i] = myAtoi(token);
                token = strtok(NULL, ":");

            }
            delayMs(10);
        }
    }
    //return 0;
}


static esp_err_t commands_handler(httpd_req_t *req)
{
    // Obtener los valores de los sensores
    
    // Crear un búfer temporal para formatear los valores de los sensores
    int sensores[3];
    sensor(sensores);

    char buffer1[100];
    char buffer2[100];
    char buffer3[100];
/*
    // Formatear los valores de los sensores en cadenas
*/
    snprintf(buffer1, sizeof(buffer1), "%d", sensores[0]);
    const char* sensor1_str = buffer1;

    snprintf(buffer2, sizeof(buffer2), "%d", sensores[1]);
    const char* sensor2_str = buffer2;
    
    snprintf(buffer3, sizeof(buffer3), "%d", sensores[2]);
    const char* sensor3_str = buffer3;

    // Construir la respuesta HTML con los datos de los sensores
    const char* html_template = "<!DOCTYPE html>\n"
                                "<html>\n"
                                "<head>\n"
                                "  <style>\n"
                                "    /* Estilos CSS adicionales */\n"
                                "    body {\n"
                                "      font-family: sans-serif;\n"
                                "      background-color: black;\n"
                                "      display: flex;\n"
                                "      justify-content: center;\n"
                                "      align-items: center;\n"
                                "      height: 100vh;\n"
                                "      margin: 0;\n"
                                "      flex-direction: column; /* Añadido para alinear verticalmente */\n"
                                "    }\n"
                                "    h1 {\n"
                                "      font-family: sans-serif;\n"
                                "      color: white;\n"
                                "    }\n"
                                "    .container {\n"
                                "      display: flex;\n"
                                "      justify-content: center;\n"
                                "      align-items: center;\n"
                                "      gap: 10px;\n"
                                "    }\n"
                                "    .mi-div {\n"
                                "      font-family: sans-serif;\n"
                                "      background-color: #565656;\n"
                                "      color: white;\n"
                                "      font-size: 18px;\n"
                                "      padding: 10px;\n"
                                "      border-radius: 50vw;\n"
                                "      width: 200px;\n"
                                "      height: 200px;\n"
                                "      display: flex;\n"
                                "      justify-content: center;\n"
                                "      align-items: center;\n"
                                "      flex-direction: column; /* Añadido para alinear verticalmente */\n"
                                "    }\n"
                                "    .subtitle {\n"
                                "      font-size: 14px;\n"
                                "      margin-top: 5px;\n"
                                "    }\n"
                                "    .value {\n"
                                "      margin-bottom: 5px;\n"
                                "    }\n"
                                "  </style>\n"
                                "    <script>\n"
                                "        setInterval(updateValues, 3000);\n"
                                "        function updateValues() {\n"
                                "            location.reload(); \n"
                                "        }\n"
                                "    </script>\n"
                                "</head>\n"
                                "<body>\n"
                                "  <h1>SENSADO</h1>\n"
                                "  <div class=\"container\">\n"
                                "    <div class=\"mi-div\">\n"
                                "      <div style=\"background-color: #343434; width: 150px; height: 150px; border-radius: 50vw; display: flex; justify-content: center; align-items: center; flex-direction: column;\">\n"
                                "          <div class=\"value\">%s °C</div>\n"
                                "        <div class=\"subtitle\">Temperatura</div>\n"
                                "      </div>\n"
                                "    </div>\n"
                                "    <div class=\"mi-div\">\n"
                                "      <div style=\"background-color: #343434; width: 150px; height: 150px; border-radius: 50vw; display: flex; justify-content: center; align-items: center; flex-direction: column;\">\n"
                                "        <div class=\"value\">%s g/m³</div>\n"
                                "        <div class=\"subtitle\">Humedad</div>\n"
                                "      </div>\n"
                                "    </div>\n"
                                "    <div class=\"mi-div\">\n"
                                "      <div style=\"background-color: #343434; width: 150px; height: 150px; border-radius: 50vw; display: flex; justify-content: center; align-items: center; flex-direction: column;\">\n"
                                "        <div class=\"value\">%s g/m³</div>\n"
                                "        <div class=\"subtitle\">Humedad de suelo</div>\n"
                                "      </div>\n"
                                "    </div>\n"
                                "  </div>\n"
                                "</body>\n"
                                "</html>";
    
    // Calcular el tamaño necesario para la cadena de respuesta
    size_t html_size = snprintf(NULL, 0, html_template, sensor1_str, sensor2_str, sensor3_str) + 1;
    
    // Crear un búfer para la respuesta HTML
    char* html_response = malloc(html_size);
    
    // Combinar la plantilla HTML y los datos de los sensores en la respuesta final
    snprintf(html_response, html_size, html_template, sensor1_str, sensor2_str, sensor3_str);
    
    // Enviar la respuesta HTTP al cliente
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html_response, HTTPD_RESP_USE_STRLEN);
    
    // Liberar la memoria asignada para la respuesta HTML
    free(html_response);
    return ESP_OK;
}

static const httpd_uri_t commands = {
    .uri      = "/commands",
    .method   = HTTP_GET,
    .handler  = commands_handler,
    .user_ctx = NULL
};


static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Iniciar el servidor httpd 
    ESP_LOGI(TAG, "Iniciando el servidor en el puerto: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Manejadores de URI
        ESP_LOGI(TAG, "Registrando manejadores de URI");
        httpd_register_uri_handler(server, &commands);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "estacion "MACSTR" se unio, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "estacion "MACSTR" se desconecto, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

esp_err_t wifi_init_softap(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Inicializacion de softAP terminada. SSID: %s password: %s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    return ESP_OK;
}


void app_main(void)
{
    httpd_handle_t server = NULL;
    uart_init(UART2_PORT, UART2_RX_PIN, UART2_TX_PIN);
    ESP_ERROR_CHECK(nvs_flash_init());
    esp_netif_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "init softAP");
    ESP_ERROR_CHECK(wifi_init_softap());
    spi_init();

    server = start_webserver();
}


