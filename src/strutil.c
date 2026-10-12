#include "strutil.h"

size_t u32_to_dec(char out[U32_DEC_MAX], uint32_t value)
{
    char tmp[U32_DEC_MAX];
    size_t n = 0;
    do {
        tmp[n++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);

    for (size_t i = 0; i < n; i++) {
        out[i] = tmp[n - 1U - i];
    }
    out[n] = '\0';
    return n;
}

size_t u32_to_hex(char out[U32_HEX_MAX], uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    out[0] = '0';
    out[1] = 'x';
    for (int i = 0; i < 8; i++) {
        out[2 + i] = digits[(value >> (28 - 4 * i)) & 0xFU];
    }
    out[10] = '\0';
    return 10;
}
