/*
 * board.h — периферия платы Nucleo-64 (F401RE / F446RE).
 *
 *   LD2  (зелёный светодиод) — PA5, активный уровень высокий
 *   B1   (синяя кнопка)      — PC13, активный уровень низкий, внешняя подтяжка
 *   USART2 TX/RX             — PA2/PA3, выведены на виртуальный COM-порт ST-LINK
 */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>
#include "regs.h"

void gpio_set_mode(gpio_t *port, unsigned pin, unsigned mode);
void gpio_set_af(gpio_t *port, unsigned pin, unsigned af);
void gpio_set_pull(gpio_t *port, unsigned pin, unsigned pull);
void gpio_set_speed(gpio_t *port, unsigned pin, unsigned speed);

void led_init(void);
void led_set(bool on);
void led_toggle(void);
bool led_is_on(void);

void button_init(void);
/* Число нажатий с прошлого вызова (с подавлением дребезга). */
uint32_t button_take_presses(void);

#endif /* BOARD_H */
