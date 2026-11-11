#ifndef USART_RING_H_
#define USART_RING_H_

#include "main.h"
#include <stdbool.h>

#define MYUART huart1

extern UART_HandleTypeDef MYUART;

uint16_t uart_available(void);
uint8_t uart_read(void);
bool uart_discard_on_error(void);
void uart_receive_irq(void);

#endif
