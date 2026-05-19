#include "fans_logic.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_parse_int(void)
{
    int value = 0;

    assert(fans_parse_int("3500", &value));
    assert(value == 3500);
    assert(fans_parse_int("+3500", &value));
    assert(value == 3500);
    assert(fans_parse_int(" 3500", &value));
    assert(value == 3500);
    assert(fans_parse_int("-1", &value));
    assert(value == -1);
    assert(!fans_parse_int("", &value));
    assert(!fans_parse_int("35x", &value));
    assert(!fans_parse_int("999999999999999999999999", &value));
    assert(!fans_parse_int(NULL, &value));
    assert(!fans_parse_int("1", NULL));
}

static void test_validate_rpm(void)
{
    assert(!fans_validate_rpm(499));
    assert(fans_validate_rpm(500));
    assert(fans_validate_rpm(3500));
    assert(fans_validate_rpm(8000));
    assert(!fans_validate_rpm(8001));
}

static void test_target_matches(void)
{
    assert(fans_target_matches(3500.0f, 3500));
    assert(fans_target_matches(3475.0f, 3500));
    assert(fans_target_matches(3525.0f, 3500));
    assert(!fans_target_matches(3474.0f, 3500));
    assert(!fans_target_matches(3526.0f, 3500));
}

static void test_force_bits(void)
{
    assert(fans_force_bits(0) == 0x0000);
    assert(fans_force_bits(1) == 0x0001);
    assert(fans_force_bits(2) == 0x0003);
    assert(fans_force_bits(4) == 0x000f);
    assert(fans_force_bits(20) == 0xffff);
}

static void test_encode_fpe2(void)
{
    unsigned char bytes[2] = {0, 0};
    fans_encode_fpe2_rpm(3500, bytes);
    assert(bytes[0] == 0x36);
    assert(bytes[1] == 0xb0);

    fans_encode_fpe2_rpm(1350, bytes);
    assert(bytes[0] == 0x15);
    assert(bytes[1] == 0x18);
}

static void test_copy_smc_key(void)
{
    char key[5];

    fans_copy_smc_key("F0Tg", key);
    assert(strcmp(key, "F0Tg") == 0);

    fans_copy_smc_key("FS!", key);
    assert(strcmp(key, "FS! ") == 0);

    fans_copy_smc_key("ABCDE", key);
    assert(strcmp(key, "ABCD") == 0);

    fans_copy_smc_key(NULL, key);
    assert(strcmp(key, "    ") == 0);
}

int main(void)
{
    test_parse_int();
    test_validate_rpm();
    test_target_matches();
    test_force_bits();
    test_encode_fpe2();
    test_copy_smc_key();

    printf("fans_logic tests passed\n");
    return 0;
}
