#include "fans_logic.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
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

static const char *saved_trim(const char *line, char *buf, size_t buflen)
{
    size_t start;
    size_t end;
    size_t len;

    if (line == NULL || buf == NULL || buflen == 0) {
        return NULL;
    }

    while (*line != '\0' && isspace((unsigned char)*line)) {
        line++;
    }

    start = 0;
    end = strlen(line);
    while (end > start && isspace((unsigned char)line[end - 1])) {
        end--;
    }

    len = end - start;
    if (len >= buflen) {
        len = buflen - 1;
    }
    memcpy(buf, line + start, len);
    buf[len] = '\0';
    return buf;
}

static void fans_saved_clear_boot(fans_saved_t *out)
{
    out->has_boot = 0;
    out->boot_sec = 0;
    out->boot_usec = 0;
}

int fans_saved_parse_line(const char *line, fans_saved_t *out)
{
    char trimmed[64];
    int rpm = 0;

    if (out == NULL) {
        return 0;
    }

    out->rpm = FANS_SAVED_AUTO;
    fans_saved_clear_boot(out);

    if (line == NULL) {
        return 0;
    }

    if (saved_trim(line, trimmed, sizeof(trimmed)) == NULL || trimmed[0] == '\0') {
        return 0;
    }

    if (strcmp(trimmed, "auto") == 0) {
        return 1;
    }

    if (!fans_parse_int(trimmed, &rpm) || !fans_validate_rpm(rpm)) {
        return 0;
    }

    out->rpm = rpm;
    return 1;
}

int fans_saved_parse_boot_line(const char *line, fans_saved_t *out)
{
    char trimmed[64];
    long boot_sec = 0;
    long boot_usec = 0;

    if (out == NULL || line == NULL) {
        return 0;
    }

    if (saved_trim(line, trimmed, sizeof(trimmed)) == NULL || trimmed[0] == '\0') {
        return 0;
    }

    if (sscanf(trimmed, "boot %ld %ld", &boot_sec, &boot_usec) != 2) {
        return 0;
    }

    out->has_boot = 1;
    out->boot_sec = boot_sec;
    out->boot_usec = boot_usec;
    return 1;
}

int fans_saved_boot_changed(const fans_saved_t *saved, long boot_sec, long boot_usec)
{
    if (saved == NULL || !saved->has_boot) {
        return 0;
    }

    return saved->boot_sec != boot_sec || saved->boot_usec != boot_usec;
}

int fans_saved_boot_action(const fans_saved_t *saved, long boot_sec, long boot_usec)
{
    if (saved == NULL || saved->rpm == FANS_SAVED_AUTO) {
        return 0;
    }

    if (!fans_validate_rpm(saved->rpm)) {
        return 0;
    }

    if (!saved->has_boot || fans_saved_boot_changed(saved, boot_sec, boot_usec)) {
        return 1;
    }

    return 0;
}

int fans_saved_format_boot_line(const fans_saved_t *saved, char *buf, size_t buflen)
{
    if (saved == NULL || buf == NULL || buflen == 0 || !saved->has_boot) {
        return 0;
    }

    return snprintf(buf, buflen, "boot %ld %ld\n", saved->boot_sec, saved->boot_usec) < (int)buflen;
}

int fans_saved_format_line(const fans_saved_t *saved, char *buf, size_t buflen)
{
    if (saved == NULL || buf == NULL || buflen == 0) {
        return 0;
    }

    if (saved->rpm == FANS_SAVED_AUTO) {
        return snprintf(buf, buflen, "auto\n") < (int)buflen;
    }

    if (!fans_validate_rpm(saved->rpm)) {
        return 0;
    }

    return snprintf(buf, buflen, "%d\n", saved->rpm) < (int)buflen;
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
