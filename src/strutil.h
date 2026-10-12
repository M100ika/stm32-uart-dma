/*
 * strutil.h — форматирование чисел без printf.
 *
 * printf из newlib тянет десятки килобайт кода и использует кучу,
 * поэтому для вывода счётчиков и адресов хватает двух простых функций.
 */
#ifndef STRUTIL_H
#define STRUTIL_H

#include <stddef.h>
#include <stdint.h>

#define U32_DEC_MAX 11U  /* "4294967295" + '\0' */
#define U32_HEX_MAX 11U  /* "0x" + 8 цифр + '\0' */

/* Пишет десятичное представление и '\0'. Возвращает длину без '\0'. */
size_t u32_to_dec(char out[U32_DEC_MAX], uint32_t value);

/* Пишет "0x" и ровно 8 шестнадцатеричных цифр. Возвращает длину. */
size_t u32_to_hex(char out[U32_HEX_MAX], uint32_t value);

#endif /* STRUTIL_H */
