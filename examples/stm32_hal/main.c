#include "inc_master.h"
#include "inc_port.h"
#include <stdio.h>

/* Транспорт */
static const inc_transport_t transport = {
    .send       = inc_port_send,
    .recv       = inc_port_recv,
    .delay_ms   = inc_port_delay,
    .get_tick_ms= inc_port_tick,
};

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();   /* UART мастера */

    /* Инициализация библиотеки */
    inc_dev_t inc;
    inc_init(&inc, &transport, 1);   /* UID = 1 */

    /* Проверка WHOAMI */
    uint16_t whoami = 0;
    if (inc_get_whoami(&inc, &whoami) == INC_OK) {
        printf("WHOAMI = 0x%04X (ожидается 0x00C1)\n", whoami);
    } else {
        printf("Устройство не отвечает\n");
    }

    float x, y, z;

    while (1)
    {
        if (inc_get_all(&inc, &x, &y, &z) == INC_OK) {
            printf("X=%.2f  Y=%.2f  Z=%.2f\n", x, y, z);
        } else {
            printf("Ошибка чтения\n");
        }

        HAL_Delay(500);
    }
}