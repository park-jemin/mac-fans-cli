#ifndef FANS_LOGIC_H
#define FANS_LOGIC_H

#include <stddef.h>
#include <stdint.h>

#define FANS_MIN_RPM 500
#define FANS_MAX_RPM 8000
#define FANS_TARGET_TOLERANCE_RPM 25
#define FANS_SAVED_AUTO (-1)

typedef struct {
    int rpm;
    int has_boot;
    long boot_sec;
    long boot_usec;
} fans_saved_t;

int fans_parse_int(const char *text, int *out);
int fans_saved_parse_line(const char *line, fans_saved_t *out);
int fans_saved_parse_boot_line(const char *line, fans_saved_t *out);
int fans_saved_format_line(const fans_saved_t *saved, char *buf, size_t buflen);
int fans_saved_format_boot_line(const fans_saved_t *saved, char *buf, size_t buflen);
int fans_saved_boot_changed(const fans_saved_t *saved, long boot_sec, long boot_usec);
/* 0 = no-op, 1 = return fans to auto (new or changed boot session) */
int fans_saved_boot_action(const fans_saved_t *saved, long boot_sec, long boot_usec);
int fans_validate_rpm(int rpm);
int fans_target_matches(float target, int rpm);
uint16_t fans_force_bits(int fan_count);
void fans_encode_fpe2_rpm(int rpm, unsigned char bytes[2]);
void fans_copy_smc_key(const char *input, char key[5]);

#endif
