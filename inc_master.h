/**
 * @file    inc_master.h
 * @brief   Библиотека-мастер для инклинометра (SCL3300 + STM32L031 + Modbus RTU).
 *
 *  Использование:
 *      1. Реализуй inc_port_send() и inc_port_recv() под свою платформу.
 *      2. Вызови inc_init(&transport, uid).
 *      3. Дёргай inc_get_x(), inc_get_y(), inc_get_z().
 */

#ifndef INC_MASTER_H
#define INC_MASTER_H

#include <stdint.h>
#include <stdbool.h>

/* ---------------- Коды ошибок ---------------- */
typedef enum {
    INC_OK              = 0,
    INC_ERR_TIMEOUT     = -1,   /* Нет ответа от устройства */
    INC_ERR_CRC         = -2,   /* CRC не совпал */
    INC_ERR_EXCEPTION   = -3,   /* Устройство вернуло Modbus-исключение */
    INC_ERR_BAD_RESP    = -4,   /* Ответ не соответствует запросу */
    INC_ERR_PARAM       = -5,   /* Неверный параметр */
} inc_status_t;

/* ---------------- Транспорт (реализует пользователь) ---------------- */
typedef struct {
    /* Отправить len байт. Возвращает 0 при успехе. */
    int  (*send)(const uint8_t *data, uint16_t len);

    /* Принять len байт с таймаутом timeout_ms. Возвращает 0 при успехе. */
    int  (*recv)(uint8_t *data, uint16_t len, uint32_t timeout_ms);

    /* Небольшая задержка между кадрами (мс). Опционально — можно NULL. */
    void (*delay_ms)(uint32_t ms);

    /* Флаг «данные пришли» для отслеживания паузы между кадрами. Опционально. */
    uint32_t (*get_tick_ms)(void);
} inc_transport_t;

/* ---------------- Контекст устройства ---------------- */
typedef struct {
    const inc_transport_t *transport;
    uint8_t  uid;            /* Modbus-адрес устройства (1..247) */
    uint32_t timeout_ms;     /* Таймаут ответа (по умолчанию 500 мс) */
} inc_dev_t;

/* ---------------- Инициализация ---------------- */
inc_status_t inc_init(inc_dev_t *dev, const inc_transport_t *transport, uint8_t uid);

/* ---------------- Чтение углов (в градусах) ---------------- */
inc_status_t inc_get_x(inc_dev_t *dev, float *angle);
inc_status_t inc_get_y(inc_dev_t *dev, float *angle);
inc_status_t inc_get_z(inc_dev_t *dev, float *angle);

/* Прочитать все три угла одной транзакцией (эффективнее) */
inc_status_t inc_get_all(inc_dev_t *dev, float *x, float *y, float *z);

/* ---------------- Чтение служебных регистров ---------------- */
inc_status_t inc_get_uid(inc_dev_t *dev, uint8_t *uid);
inc_status_t inc_get_spi_speed(inc_dev_t *dev, uint8_t *idx);
inc_status_t inc_get_whoami(inc_dev_t *dev, uint16_t *whoami);

/* ---------------- Запись ---------------- */
inc_status_t inc_set_uid(inc_dev_t *dev, uint8_t new_uid);
inc_status_t inc_set_spi_speed(inc_dev_t *dev, uint8_t idx);

/* ---------------- Вспомогательные ---------------- */
const char *inc_strerror(inc_status_t status);

#endif /* INC_MASTER_H */