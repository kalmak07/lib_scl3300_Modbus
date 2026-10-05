# Инклинометр — библиотека `inc_master`

Клиентская библиотека для работы с инклинометром по RS-485 (Modbus RTU).

## Modbus-регистры

| Адрес | Название | Доступ | Формат |
| :--- | :--- | :--- | :--- |
| 0x0000–0x0001 | ANGLE_X | R | Float (ABCD) |
| 0x0002–0x0003 | ANGLE_Y | R | Float (ABCD) |
| 0x0004–0x0005 | ANGLE_Z | R | Float (ABCD) |
| 0x0006 | UID | R/W | 1..247 (EEPROM) |
| 0x0007 | SPI_SPEED | R/W | 0..5 (EEPROM) |
| 0x0008 | WHOAMI | R | 0x00C1 |

Функции: `0x03`, `0x06`, `0x10`.

## Подключение

Реализуй 4 функции в `inc_port.c`:

```c
int      inc_port_send(const uint8_t *data, uint16_t len);
int      inc_port_recv(uint8_t *data, uint16_t len, uint32_t timeout_ms);
void     inc_port_delay(uint32_t ms);
uint32_t inc_port_tick(void);
```

## API

**Инициализация**
```c
inc_status_t inc_init(inc_dev_t *dev, const inc_transport_t *transport, uint8_t uid);
```

**Чтение углов (градусы)**
```c
inc_status_t inc_get_x(inc_dev_t *dev, float *angle);
inc_status_t inc_get_y(inc_dev_t *dev, float *angle);
inc_status_t inc_get_z(inc_dev_t *dev, float *angle);
inc_status_t inc_get_all(inc_dev_t *dev, float *x, float *y, float *z);
```
`inc_get_all()` эффективнее — читает 3 угла одним кадром.

**Служебные регистры**
```c
inc_status_t inc_get_uid(inc_dev_t *dev, uint8_t *uid);
inc_status_t inc_get_spi_speed(inc_dev_t *dev, uint8_t *idx);
inc_status_t inc_get_whoami(inc_dev_t *dev, uint16_t *whoami);
```

**Запись**
```c
inc_status_t inc_set_uid(inc_dev_t *dev, uint8_t new_uid);
inc_status_t inc_set_spi_speed(inc_dev_t *dev, uint8_t idx);
```
После `inc_set_uid()` адрес в `dev->uid` обновляется автоматически.

**Ошибки**
```c
const char *inc_strerror(inc_status_t status);
```

| Код | Значение |
| :--- | :--- |
| INC_OK | Успех |
| INC_ERR_TIMEOUT | Нет ответа |
| INC_ERR_CRC | Ошибка CRC |
| INC_ERR_EXCEPTION | Modbus-исключение |
| INC_ERR_BAD_RESP | Ответ не соответствует запросу |
| INC_ERR_PARAM | Неверный параметр |

## Пример

```c
#include "inc_master.h"
#include "inc_port.h"

static const inc_transport_t transport = {
    .send        = inc_port_send,
    .recv        = inc_port_recv,
    .delay_ms    = inc_port_delay,
    .get_tick_ms = inc_port_tick,
};

inc_dev_t inc;
inc_init(&inc, &transport, 1);

float x, y, z;
inc_get_all(&inc, &x, &y, &z);
```