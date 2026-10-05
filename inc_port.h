/**
 * @file    inc_port.h
 * @brief   Пример порта для STM32 HAL (USART + RS-485 DE).
 *          Замени на свои функции, если используешь другую платформу.
 */

#ifndef INC_PORT_H
#define INC_PORT_H

#include "inc_master.h"

/* Твои хэндлы */
extern UART_HandleTypeDef huart1;
extern SPI_HandleTypeDef  hspi_unused;  /* заглушка, если нужна */

/* Пины управления DE/RE твоего мастера */
#define MASTER_DE_PORT   GPIOA
#define MASTER_DE_PIN    GPIO_PIN_1
#define MASTER_RE_PORT   GPIOA
#define MASTER_RE_PIN    GPIO_PIN_4

/* Прототипы */
int  inc_port_send(const uint8_t *data, uint16_t len);
int  inc_port_recv(uint8_t *data, uint16_t len, uint32_t timeout_ms);
void inc_port_delay(uint32_t ms);
uint32_t inc_port_tick(void);

#endif