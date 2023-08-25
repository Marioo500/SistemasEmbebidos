#include "uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


void uart_init(uart_port_t uart_num, uint8_t rxPin, uint8_t txPin)
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

bool uartKbhit(uart_port_t uart_num)
{
    uint8_t length;
    uart_get_buffered_data_len(uart_num, (size_t*)&length);
    return (length > 0);
}

char uartGetchar(uart_port_t uart_num)
{
    char c;
    // Wait for a received byte
    while(!uartKbhit(uart_num))
    {
         vTaskDelay(10/portTICK_PERIOD_MS);
    }
    // read byte, no wait
    uart_read_bytes(uart_num, &c, sizeof(c), 0);
    return c;
}

void uartGets(uart_port_t uart_num, char *str)
{
    uint8_t i = 0;
    char *tmp = str;
    char c;
    do
    {
        c = uartGetchar(uart_num);
        if (c != '\r')
        {
            if(c != 8)
            {
                *str = c;
                str++;
                i++;
                uartPutchar(uart_num, c);
            }
            else 
            {
                if ( str > tmp)
                {
                    str--;
                    uartPutchar(uart_num, 8);
                    uartPutchar(uart_num, 32);
                    uartPutchar(uart_num, 8);
                }
            }
        }    
    } while (c != '\r');
    *str = '\0';
}

void uartPuts(uart_port_t uart_num, char *str)
{ 
    while (*str)
    {
        uartPutchar(uart_num , *str);
        str++;
    }
}

void uartPutchar(uart_port_t uart_num, char c)
{
    uart_write_bytes(uart_num, &c, sizeof(c));
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