#include <string.h>

#include "strutil.h"
#include "test.h"

static void decimal(void)
{
    char b[U32_DEC_MAX];
    CHECK_EQ(u32_to_dec(b, 0), 1);
    CHECK(strcmp(b, "0") == 0);
    CHECK_EQ(u32_to_dec(b, 1234567), 7);
    CHECK(strcmp(b, "1234567") == 0);
    CHECK_EQ(u32_to_dec(b, 4294967295U), 10);
    CHECK(strcmp(b, "4294967295") == 0);
}

static void hex(void)
{
    char b[U32_HEX_MAX];
    CHECK_EQ(u32_to_hex(b, 0), 10);
    CHECK(strcmp(b, "0x00000000") == 0);
    u32_to_hex(b, 0xDEADC0DEU);
    CHECK(strcmp(b, "0xDEADC0DE") == 0);
}

void run_strutil_tests(void)
{
    RUN(decimal);
    RUN(hex);
}
