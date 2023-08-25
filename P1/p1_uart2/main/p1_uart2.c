#include <stdio.h>
#include <string.h>
#include <time.h>
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

#define LED_GPIO 22

typedef struct 
{
    uint8_t cabecera;
    uint8_t comando;
    uint8_t data_length_low;
    uint8_t data_length_high;
    uint8_t data_l;
    uint8_t data_h;
    uint8_t fin;
    uint32_t crc32_end_cabecera;
} crc32_struct;

//Funciones auxiliares
const char encendido[] = "ON";
const char apagado[] = "OFF";

uint8_t ledstate = 1;

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

void delayMs(uint16_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
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

uint32_t myAtoi(char *str) // TERMINADA
{
    uint32_t number = 0;
    uint16_t i = 0;
    while(str[i] >= '0' && str[i] <= '9')
    {
        number = number * 10 + str[i] - '0';
        i++;
    }
    return number;
}

void myItoa(uint32_t number, char* str, uint8_t base) //TERMINADA
{
        uint16_t i = 0;
    if (!number)
    {
        str[0] = '0';
        i = 1;
    }
     
    while(number)
    {
        uint32_t r = number % base;
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

char uartGetchar(uart_port_t uart_num)
{
    char c;
    // Wait for a received byte
    while(!uartKbhit(uart_num))
    {
        delayMs(10);
    }
    // read byte, no wait
    uart_read_bytes(uart_num, &c, sizeof(c), 0);
    return c;
}

void uartPutchar(uart_port_t uart_num, char c)
{
    uart_write_bytes(uart_num, &c, sizeof(c));
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

unsigned reverse_crc32(unsigned x) {
   x = ((x & 0x55555555) <<  1) | ((x >>  1) & 0x55555555);
   x = ((x & 0x33333333) <<  2) | ((x >>  2) & 0x33333333);
   x = ((x & 0x0F0F0F0F) <<  4) | ((x >>  4) & 0x0F0F0F0F);
   x = (x << 24) | ((x & 0xFF00) << 8) |
       ((x >> 8) & 0xFF00) | (x >> 24);
   return x;
}

unsigned int crc32a(crc32_struct* paquete) {
    int j;
    unsigned int byte, crc;
    uint8_t *ptr = &(paquete->cabecera);
    crc = 0xFFFFFFFF;
    while (ptr != &(paquete->fin)) 
    {
        byte = *ptr;            // Get next byte.
        byte = reverse_crc32(byte);         // 32-bit reversal.
        for (j = 0; j <= 7; j++)
        {    // Do eight times.
            if ((int)(crc ^ byte) < 0)
                crc = (crc << 1) ^ 0x04C11DB7;
            else crc = crc << 1;
                byte = byte << 1;          // Ready next msg bit.
        }
      ptr++;
    }
   return reverse_crc32(~crc);
}

void uartPuts(uart_port_t uart_num, char *str)
{ 
    while (*str)
    {
        uartPutchar(uart_num , *str);
        str++;
    }
}

void app_main(void)
{
    crc32_struct datos;
    char nl[] = "\n\n";
    uint8_t num;
    uint8_t *ptr, *ptr_aux;
    uint8_t *ptr_const = &datos.cabecera;
    uint32_t compare, tiempo;
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, ledstate);
    char cad[20];
    char cad_to_send[20];
    uart_init(UART2_PORT, UART2_RX_PIN, UART2_TX_PIN);
    uart_init(PC_UART_PORT, PC_UART_RX_PIN, PC_UART_TX_PIN);
    ptr = &datos.cabecera;
    ptr_aux = &datos.fin;
    while (1)
    {
        if(uartKbhit(UART2_PORT))
        {
            int len = uart_read_bytes(UART2_PORT, cad, (READ_BUF_SIZE - 1), 0);
            if (len)
            {
                if(ptr <= ptr_aux)
                {
                    num = myAtoi(cad);
                    *ptr = num;
                    ptr++;
                }
                else
                {
                    
                    ptr = ptr_const;
                    datos.crc32_end_cabecera = myAtoi(cad);
                    myItoa(datos.crc32_end_cabecera, cad, 16);
                    compare = crc32a(&datos);
                    uartPuts(PC_UART_PORT, cad); //CRC32 que llega
                    myItoa(compare, cad, 16);
                    uartPuts(PC_UART_PORT, nl);
                    uartPuts(PC_UART_PORT, cad); //CRC32 que se crea 
                    if (datos.crc32_end_cabecera == compare )
                    {
                        
                        switch (datos.comando)
                        {
                            case 0x10: //Enviar timestamp
                                tiempo = time(NULL);
                                myItoa(tiempo, cad_to_send,10);
                                datos.data_l = tiempo;

                            break;
                            case 0x11: //Enviar estado del led GPIO
                                i
                                {
                                    datos.data_l = 1
                                }
                            break;
                            case 0x12:  //Enviar temperatura
                                datos.data_l = 25;
                                myItoa( datos.data_l, cad_to_send,10);
                                uart_write_bytes(UART2_PORT, (const char*)cad_to_send, sizeof(cad_to_send));

                            break;
                            case 0x13:
                                ledstate = !ledstate;
                                gpio_set_level(LED_GPIO, ledstate);
                            break;
                            default:
                            break;
                        }
                        compare = crc32a(&datos);
                        datos.crc32_end_cabecera = compare;
                        ptr = &(datos.cabecera);
                        for (uint8_t j = 0; j < 7; j++)
                        {
                            myItoa(*ptr, cad_to_send, 10);
                            uart_write_bytes(UART2_PORT, (const char*)cad_to_send, sizeof(cad_to_send));
                            myItoa(*ptr, cad_to_send, 16);
                            uartPuts(PC_UART_PORT, cad_to_send);
                            ptr++;
                            delayMs(250);
                        }
                        myItoa(datos.crc32_end_cabecera, cad_to_send, 10);
                        uart_write_bytes(UART2_PORT, (const char*)cad_to_send, sizeof(cad_to_send));
                        delayMs(250);
                        ptr = ptr_const;
                    }
                }
            }
        }
        delayMs(10);
    }
    
}
