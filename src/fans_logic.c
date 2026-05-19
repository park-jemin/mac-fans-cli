#include "fans_logic.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

int fans_parse_int(const char *text, int *out)
{
    char *end = NULL;
    long value;

    if (text == NULL || *text == '\0' || out == NULL) {
        return 0;
    }

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < INT_MIN || value > INT_MAX) {
        return 0;
    }

    *out = (int)value;
    return 1;
}

int fans_validate_rpm(int rpm)
{
    return rpm >= FANS_MIN_RPM && rpm <= FANS_MAX_RPM;
}

int fans_target_matches(float target, int rpm)
{
    return target >= (float)(rpm - FANS_TARGET_TOLERANCE_RPM) &&
           target <= (float)(rpm + FANS_TARGET_TOLERANCE_RPM);
}

uint16_t fans_force_bits(int fan_count)
{
    uint16_t bits = 0;
    int limit = fan_count > 16 ? 16 : fan_count;

    for (int i = 0; i < limit; i++) {
        bits |= (uint16_t)(1u << i);
    }

    return bits;
}

void fans_encode_fpe2_rpm(int rpm, unsigned char bytes[2])
{
    uint16_t encoded = (uint16_t)(rpm << 2);
    bytes[0] = (unsigned char)((encoded >> 8) & 0xFF);
    bytes[1] = (unsigned char)(encoded & 0xFF);
}

void fans_copy_smc_key(const char *input, char key[5])
{
    size_t len = input == NULL ? 0 : strlen(input);
    size_t copy_len = len < 4 ? len : 4;

    memset(key, ' ', 4);
    if (copy_len > 0) {
        memcpy(key, input, copy_len);
    }
    key[4] = '\0';
}
