#ifndef _MY_UART_H_
#define _MY_UART_H_

#include "driver/uart.h"


#define UART1_PORT      (1)
#define UART1_RX_PIN    (18)
#define UART1_TX_PIN    (19)

#define UART2_PORT      (2)
#define UART2_RX_PIN    (16)
#define UART2_TX_PIN    (17)

#define UART_BAUD_RATE          (115200)
#define TASK_STACK_SIZE         (1048)
#define READ_BUF_SIZE           (1024)


void uart_init(uart_port_t uart_num, uint8_t rxPin, uint8_t txPin);
void swap(char *x, char *y);
char* reverse(char *buffer, int i, int j);
bool uartKbhit(uart_port_t uart_num);
char uartGetchar(uart_port_t uart_num);
void uartGets(uart_port_t uart_num, char *str);
void uartPuts(uart_port_t uart_num, char *str);
void uartPutchar(uart_port_t uart_num, char c);
uint16_t myAtoi(char *str);



#endif