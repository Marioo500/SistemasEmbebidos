#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"


// UART 0
#define PC_UART_PORT    (0)
#define PC_UART_RX_PIN  (3)
#define PC_UART_TX_PIN  (1)
// UART 1
#define UART1_PORT      (1)
#define UART1_RX_PIN    (18)
#define UART1_TX_PIN    (19)
// UART 2
#define UART2_PORT      (2)
#define UART2_RX_PIN    (16)
#define UART2_TX_PIN    (17)

#define UART_BAUD_RATE          (115200) 
#define TASK_STACK_SIZE         (1048)
#define READ_BUF_SIZE           (1024)

static void delayMs(uint32_t ms)
{
    vTaskDelay(ms/portTICK_PERIOD_MS);
}

static void uart_init(uart_port_t uart_num, uint8_t rxPin, uint8_t txPin)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(uart_num, READ_BUF_SIZE, READ_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void uartClrScr(uart_port_t uart_num)
{
    // Uso "const" para sugerir que el contenido del arreglo lo coloque en Flash y no en RAM
    const char caClearScr[] = "\033[H\033[2J";
    uart_write_bytes(uart_num, caClearScr, sizeof(caClearScr));
}

void uartGotoxy(uart_port_t uart_num, uint8_t x, uint8_t y) //TERMINADA
{
    char caGotoxy[10] = {'\e','['};
    char cad2[3];
    uint8_t a = 2;
    uint8_t b = 0;
    while(x > 0)
    {
        cad2[b] = (char)(x % 10) + 48;
        b++;
        x/=10;
    }
    while(b > 0)
    {
        caGotoxy[a] = cad2[b-1];
        a++;
        b--;
    }
    caGotoxy[a] = ';';
    a++;
    while(y > 0)
    {
        cad2[b] = (char)(y % 10) + 48;
        b++;
        y/=10;
    }
    while(b > 0)
    {
        caGotoxy[a] = cad2[b-1];
        a++;
        b--;
    }
    caGotoxy[a] = 'H';
    uart_write_bytes(uart_num, caGotoxy, sizeof(caGotoxy));
}

void swap(char *x, char *y) 
{
    char t = *x; *x = *y; *y = t;
}

char* reverse(char *buffer, int i, int j)
{
    while (i < j) {
        swap(&buffer[i++], &buffer[j--]);
    }
 
    return buffer;
}

uint16_t myAtoi(char *str) // TERMINADA
{
    uint16_t number = 0;
    uint16_t i = 0;
    while(str[i] >= '0' && str[i] <= '9')
    {
        number = number * 10 + str[i] - '0';
        i++;
    }
    return number;
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

bool uartKbhit(uart_port_t uart_num)
{
    uint8_t length;
    uart_get_buffered_data_len(uart_num, (size_t*)&length);
    return (length > 0);
}

void app_main(void)
{
    uartGetchar(PC_UART_PORT);
    const char cad[20] = "Introduce codigo: ";
    char cad_aux[20];
    uart_init(UART2_PORT, UART2_RX_PIN, UART2_TX_PIN);
    uart_init(PC_UART_PORT, PC_UART_RX_PIN, PC_UART_TX_PIN);
    while (1)
    {
        uartClrScr(PC_UART_PORT);
        uart_write_bytes(PC_UART_PORT, (const char *) cad, sizeof(cad));
        uart_write_bytes(UART2_PORT, (const char *) cad_aux, sizeof(cad_aux));
        //se manda a dispositivo 2 y espera una respuesta
        delayMs(500);
        if(uartKbhit(UART2_PORT))
        {
            int len = uart_read_bytes(UART2_PORT, cad_aux, (READ_BUF_SIZE - 1), 0);
            if (len)
            {
                uartGotoxy(PC_UART_PORT, 5,5);
                uart_write_bytes(PC_UART_PORT, (const char *) cad_aux, len);
                delayMs(2500);
                
            }

        }
    }
    
}
