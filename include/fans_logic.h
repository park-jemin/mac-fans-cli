#ifndef FANS_LOGIC_H
#define FANS_LOGIC_H

#include <stdint.h>

#define FANS_MIN_RPM 500
#define FANS_MAX_RPM 8000
#define FANS_TARGET_TOLERANCE_RPM 25

int fans_parse_int(const char *text, int *out);
int fans_validate_rpm(int rpm);
int fans_target_matches(float target, int rpm);
uint16_t fans_force_bits(int fan_count);
void fans_encode_fpe2_rpm(int rpm, unsigned char bytes[2]);
void fans_copy_smc_key(const char *input, char key[5]);

#endif
