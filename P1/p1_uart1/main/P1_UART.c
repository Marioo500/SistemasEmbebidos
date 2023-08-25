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

typedef struct 
{
    uint8_t  cabecera;
    uint8_t  comando;
    uint8_t  data_length_low;
    uint8_t  data_length_high;
    uint8_t  data_l;
    uint8_t  data_h;
    uint8_t  fin;
    uint32_t crc32_end_cabecera;

} crc32_struct;

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
    uint8_t *ptr , i, num;
    i = 0;
    datos.cabecera = 0x5A;
    datos.fin = 0xB2;
    const char cad[] = "Introduce codigo: \n1)0x10\n2)0x11\n3)0x12\n4)0x13\n";
    char a;
    uint32_t tmp;
    char cad_aux[20];
    char cad_to_send[20];
    uart_init(UART2_PORT, UART2_RX_PIN, UART2_TX_PIN);
    uart_init(PC_UART_PORT, PC_UART_RX_PIN, PC_UART_TX_PIN);
    while (1)
    {
        uartClrScr(PC_UART_PORT);
        uart_write_bytes(PC_UART_PORT, (const char *) cad, sizeof(cad));
        a = uartGetchar(PC_UART_PORT);
        switch (a)
        {
            case '1':
                datos.comando = 0x10;
                break;
            case '2':
                datos.comando = 0x11;
                break;
            case '3':
                datos.comando = 0x12;
                break;
            case '4':
                datos.comando = 0x13;
                break;
            default:
                break;
        }
        datos.data_length_high = 0;
        datos.data_length_low = 0;
        datos.data_h =  0;
        datos.data_l = 0;
        ptr = &datos.cabecera;
        tmp = crc32a(&datos);
        datos.crc32_end_cabecera = tmp;
        ptr = &datos.cabecera;
        for (uint8_t j = 0; j < 7; j++)
        {
            myItoa(*ptr, cad_to_send, 10);
            uart_write_bytes(UART2_PORT, (const char*)cad_to_send, sizeof(cad_to_send));
            ptr++;
            delayMs(250);
        }
        myItoa(datos.crc32_end_cabecera, cad_to_send, 10);
        uart_write_bytes(UART2_PORT, (const char*)cad_to_send, sizeof(cad_to_send));
        tmp = myAtoi(cad_to_send);
        myItoa(tmp, cad_aux, 16);
        //se manda a dispositivo 2 y espera una respuesta
        ptr = &datos.cabecera;
        delayMs(500);
        uartGotoxy(PC_UART_PORT, 10,10);
        if(uartKbhit(UART2_PORT))
        {
            int len = uart_read_bytes(UART2_PORT, cad_aux, (READ_BUF_SIZE - 1), 0);
            if (len)
            {
                num = myAtoi(cad_aux);
                uartPuts(PC_UART_PORT, cad_aux); //imprime el comando
                *ptr = num;
                ptr++;
                i++;
                if (i == 7)
                {
                    i = 0;
                    datos.crc32_end_cabecera = myAtoi(cad_aux);
                    tmp = crc32a(&datos);
                    if (datos.crc32_end_cabecera == tmp)
                    {
                        uartGotoxy(PC_UART_PORT, 5,5);
                        myItoa(datos.data_l, cad_aux, 10);
                        uart_write_bytes(PC_UART_PORT, (const char*)cad_aux, datos.data_length_low);
                        myItoa(datos.data_h, cad_aux, 10);
                        uart_write_bytes(PC_UART_PORT, (const char*)cad_aux, datos.data_length_high);
                    }
                }
            }
        }
        delayMs(1000);
        delayMs(10);
    }
}