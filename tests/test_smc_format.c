#include "smc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_key_roundtrip(void)
{
    char key[5];
    UInt32 value = smc_key_to_uint32("F0Tg", 4, 16);
    smc_uint32_to_key(key, value);
    assert(strcmp(key, "F0Tg") == 0);
}

static void test_fpe2_decode(void)
{
    unsigned char bytes[2] = {0x36, 0xb0};
    assert(smc_fpe_to_float(bytes, 2, 2) == 3500.0f);
}

static void test_value_to_float(void)
{
    SMCVal_t val;
    memset(&val, 0, sizeof(val));

    val.dataSize = 4;
    strcpy(val.dataType, DATATYPE_FLT);
    float rpm = 3500.0f;
    memcpy(val.bytes, &rpm, sizeof(rpm));
    assert(smc_value_to_float(val) == 3500.0f);

    memset(&val, 0, sizeof(val));
    val.dataSize = 2;
    strcpy(val.dataType, DATATYPE_FPE2);
    val.bytes[0] = 0x36;
    val.bytes[1] = 0xb0;
    assert(smc_value_to_float(val) == 3500.0f);
}

int main(void)
{
    test_key_roundtrip();
    test_fpe2_decode();
    test_value_to_float();

    printf("smc_format tests passed\n");
    return 0;
}
