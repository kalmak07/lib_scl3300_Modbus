/**
 * @file    inc_port.c
 * @brief   Реализация транспорта для STM32 HAL (USART + RS-485).
 */

#include "inc_port.h"

int inc_port_send(const uint8_t *data, uint16_t len)
{
    /* Переключаем ADM2582E мастера в режим передачи */
    HAL_GPIO_WritePin(MASTER_DE_PORT, MASTER_DE_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MASTER_RE_PORT, MASTER_RE_PIN, GPIO_PIN_SET);
    for (volatile int i = 0; i < 500; i++) { __NOP(); }

    HAL_StatusTypeDef st = HAL_UART_Transmit(&huart1, (uint8_t *)data, len,
                                             HAL_MAX_DELAY);

    while (!(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC))) { }

    for (volatile int i = 0; i < 500; i++) { __NOP(); }
    HAL_GPIO_WritePin(MASTER_DE_PORT, MASTER_DE_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MASTER_RE_PORT, MASTER_RE_PIN, GPIO_PIN_RESET);

    return (st == HAL_OK) ? 0 : -1;
}

int inc_port_recv(uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    /* Простой блокирующий приём с таймаутом */
    uint32_t start = HAL_GetTick();
    uint16_t got = 0;

    while (got < len) {
        uint8_t byte;
        if (HAL_UART_Receive(&huart1, &byte, 1, 10) == HAL_OK) {
            data[got++] = byte;
        } else {
            if ((HAL_GetTick() - start) >= timeout_ms) return -1;
        }
    }
    return 0;
}

void inc_port_delay(uint32_t ms)
{
    HAL_Delay(ms);
}

uint32_t inc_port_tick(void)
{
    return HAL_GetTick();
}