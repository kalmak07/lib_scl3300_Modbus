/**
 * @file    inc_master.c
 * @brief   Реализация клиента Modbus RTU для инклинометра.
 */

#include "inc_master.h"
#include <string.h>

/* ---------------- Modbus коды ---------------- */
#define MB_FC_READ_HOLDING   0x03
#define MB_FC_WRITE_SINGLE   0x06

/* ---------------- Карта регистров ---------------- */
#define REG_ANGLE_X_HI       0x0000
#define REG_ANGLE_X_LO       0x0001
#define REG_ANGLE_Y_HI       0x0002
#define REG_ANGLE_Y_LO       0x0003
#define REG_ANGLE_Z_HI       0x0004
#define REG_ANGLE_Z_LO       0x0005
#define REG_UID              0x0006
#define REG_SPI_SPEED        0x0007
#define REG_WHOAMI           0x0008

/* ---------------- Внутренние константы ---------------- */
#define INC_DEFAULT_TIMEOUT_MS   500
#define INC_GAP_MS               5      /* пауза между кадрами Modbus RTU */

/* ---------------- CRC16 (Modbus) ---------------- */

static uint16_t crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else         crc >>= 1;
        }
    }
    return crc;
}

/* ---------------- Транзакция ---------------- */

/**
 * @brief  Обмен кадром с устройством.
 * @param  dev          контекст
 * @param  request      кадр запроса (без CRC)
 * @param  req_len      длина запроса
 * @param  resp         буфер для ответа (включая CRC)
 * @param  resp_max     размер буфера
 * @param  resp_len     [out] фактическая длина ответа
 */
static inc_status_t transaction(inc_dev_t *dev,
                                const uint8_t *request, uint16_t req_len,
                                uint8_t *resp, uint16_t resp_max,
                                uint16_t *resp_len)
{
    if (!dev || !dev->transport) return INC_ERR_PARAM;
    if (req_len + 2 > resp_max)   return INC_ERR_PARAM;

    /* Собираем кадр: запрос + CRC */
    uint8_t tx[16];
    if (req_len + 2 > sizeof(tx)) return INC_ERR_PARAM;

    memcpy(tx, request, req_len);
    uint16_t crc = crc16(tx, req_len);
    tx[req_len]     = crc & 0xFF;
    tx[req_len + 1] = crc >> 8;
    uint16_t tx_len = req_len + 2;

    /* Пауза перед запросом — чтобы устройство точно увидело начало кадра */
    if (dev->transport->delay_ms) {
        dev->transport->delay_ms(INC_GAP_MS);
    }

    /* Отправка */
    if (dev->transport->send(tx, tx_len) != 0) {
        return INC_ERR_TIMEOUT;
    }

    /* Приём первых 2 байт (адрес + функция) — чтобы понять, что ответ есть */
    if (dev->transport->recv(resp, 2, dev->timeout_ms) != 0) {
        return INC_ERR_TIMEOUT;
    }

    if (resp[0] != dev->uid) return INC_ERR_BAD_RESP;

    /* Исключение? */
    if (resp[1] & 0x80) {
        /* Читаем оставшиеся 3 байта (код ошибки + CRC) */
        if (dev->transport->recv(resp + 2, 3, dev->timeout_ms) != 0) {
            return INC_ERR_TIMEOUT;
        }
        return INC_ERR_EXCEPTION;
    }

    if (resp[1] != request[1]) return INC_ERR_BAD_RESP;

    /* Дальше зависит от функции. Пока — простой приём «до конца по времени» */
    /* Читаем оставшиеся байты в цикле, ориентируясь на паузу */
    uint16_t got = 2;

    /* Минимум 5 байт (для FC 0x06) и 5+ для FC 0x03 */
    if (dev->transport->recv(resp + 2, 3, dev->timeout_ms) != 0) {
        return INC_ERR_TIMEOUT;
    }
    got = 5;

    /* Определяем полную длину ответа для FC 0x03 */
    if (resp[1] == MB_FC_READ_HOLDING) {
        uint8_t byte_count = resp[2];
        uint16_t full_len = 3 + byte_count + 2;  /* addr+fc+bc + data + crc */

        if (full_len > resp_max) return INC_ERR_BAD_RESP;

        if (got < full_len) {
            if (dev->transport->recv(resp + got, full_len - got,
                                     dev->timeout_ms) != 0) {
                return INC_ERR_TIMEOUT;
            }
            got = full_len;
        }
    }

    *resp_len = got;

    /* Проверка CRC */
    uint16_t rx_crc = resp[got - 2] | (resp[got - 1] << 8);
    uint16_t calc_crc = crc16(resp, got - 2);
    if (rx_crc != calc_crc) return INC_ERR_CRC;

    return INC_OK;
}

/* ---------------- Внутренние хелперы ---------------- */

static inc_status_t read_regs(inc_dev_t *dev, uint16_t start,
                              uint16_t count, uint16_t *regs)
{
    uint8_t req[6] = {
        dev->uid,
        MB_FC_READ_HOLDING,
        (uint8_t)(start >> 8), (uint8_t)(start & 0xFF),
        (uint8_t)(count >> 8), (uint8_t)(count & 0xFF),
    };

    uint8_t  resp[64];
    uint16_t resp_len = 0;

    inc_status_t st = transaction(dev, req, 6, resp, sizeof(resp), &resp_len);
    if (st != INC_OK) return st;

    uint8_t byte_count = resp[2];
    if (byte_count != count * 2) return INC_ERR_BAD_RESP;

    for (uint16_t i = 0; i < count; i++) {
        regs[i] = ((uint16_t)resp[3 + i * 2] << 8) | resp[4 + i * 2];
    }
    return INC_OK;
}

static inc_status_t write_reg(inc_dev_t *dev, uint16_t reg, uint16_t val)
{
    uint8_t req[6] = {
        dev->uid,
        MB_FC_WRITE_SINGLE,
        (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF),
        (uint8_t)(val >> 8), (uint8_t)(val & 0xFF),
    };

    uint8_t  resp[16];
    uint16_t resp_len = 0;

    return transaction(dev, req, 6, resp, sizeof(resp), &resp_len);
}

static float regs_to_float(uint16_t hi, uint16_t lo)
{
    union { float f; uint32_t u; } u;
    u.u = ((uint32_t)hi << 16) | lo;
    return u.f;
}

/* ---------------- Публичные функции ---------------- */

inc_status_t inc_init(inc_dev_t *dev, const inc_transport_t *transport, uint8_t uid)
{
    if (!dev || !transport) return INC_ERR_PARAM;
    if (uid < 1 || uid > 247) return INC_ERR_PARAM;

    dev->transport  = transport;
    dev->uid        = uid;
    dev->timeout_ms = INC_DEFAULT_TIMEOUT_MS;

    return INC_OK;
}

inc_status_t inc_get_all(inc_dev_t *dev, float *x, float *y, float *z)
{
    if (!dev) return INC_ERR_PARAM;

    uint16_t regs[6];
    inc_status_t st = read_regs(dev, REG_ANGLE_X_HI, 6, regs);
    if (st != INC_OK) return st;

    if (x) *x = regs_to_float(regs[0], regs[1]);
    if (y) *y = regs_to_float(regs[2], regs[3]);
    if (z) *z = regs_to_float(regs[4], regs[5]);
    return INC_OK;
}

inc_status_t inc_get_x(inc_dev_t *dev, float *angle)
{
    return inc_get_all(dev, angle, NULL, NULL);
}

inc_status_t inc_get_y(inc_dev_t *dev, float *angle)
{
    return inc_get_all(dev, NULL, angle, NULL);
}

inc_status_t inc_get_z(inc_dev_t *dev, float *angle)
{
    return inc_get_all(dev, NULL, NULL, angle);
}

inc_status_t inc_get_uid(inc_dev_t *dev, uint8_t *uid)
{
    uint16_t reg = 0;
    inc_status_t st = read_regs(dev, REG_UID, 1, &reg);
    if (st != INC_OK) return st;
    if (uid) *uid = (uint8_t)reg;
    return INC_OK;
}

inc_status_t inc_get_spi_speed(inc_dev_t *dev, uint8_t *idx)
{
    uint16_t reg = 0;
    inc_status_t st = read_regs(dev, REG_SPI_SPEED, 1, &reg);
    if (st != INC_OK) return st;
    if (idx) *idx = (uint8_t)reg;
    return INC_OK;
}

inc_status_t inc_get_whoami(inc_dev_t *dev, uint16_t *whoami)
{
    uint16_t reg = 0;
    inc_status_t st = read_regs(dev, REG_WHOAMI, 1, &reg);
    if (st != INC_OK) return st;
    if (whoami) *whoami = reg;
    return INC_OK;
}

inc_status_t inc_set_uid(inc_dev_t *dev, uint8_t new_uid)
{
    if (new_uid < 1 || new_uid > 247) return INC_ERR_PARAM;

    inc_status_t st = write_reg(dev, REG_UID, new_uid);
    if (st != INC_OK) return st;

    /* Устройство теперь отвечает по новому адресу */
    dev->uid = new_uid;
    return INC_OK;
}

inc_status_t inc_set_spi_speed(inc_dev_t *dev, uint8_t idx)
{
    if (idx > 5) return INC_ERR_PARAM;
    return write_reg(dev, REG_SPI_SPEED, idx);
}

const char *inc_strerror(inc_status_t status)
{
    switch (status) {
    case INC_OK:            return "OK";
    case INC_ERR_TIMEOUT:   return "Timeout";
    case INC_ERR_CRC:       return "CRC error";
    case INC_ERR_EXCEPTION: return "Modbus exception";
    case INC_ERR_BAD_RESP:  return "Bad response";
    case INC_ERR_PARAM:     return "Bad parameter";
    default:                return "Unknown";
    }
}