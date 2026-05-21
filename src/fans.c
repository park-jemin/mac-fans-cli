/*
 * Apple SMC fan control CLI.
 * Based on smcFanControl by devnull & Hendrik Holtmann.
 * GPL License.
 */

#include "fans.h"

#include "fans_logic.h"
#include "smc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

UInt32 smc_key_to_uint32(char *str, int size, int base)
{
    UInt32 total = 0;

    for (int i = 0; i < size; i++) {
        if (base == 16) {
            total += str[i] << (size - 1 - i) * 8;
        } else {
            total += ((unsigned char)str[i] << (size - 1 - i) * 8);
        }
    }

    return total;
}

void smc_uint32_to_key(char *str, UInt32 val)
{
    str[0] = '\0';
    sprintf(str, "%c%c%c%c",
            (unsigned int)val >> 24,
            (unsigned int)val >> 16,
            (unsigned int)val >> 8,
            (unsigned int)val);
}

float smc_fpe_to_float(unsigned char *str, int size, int e)
{
    float total = 0;

    for (int i = 0; i < size; i++) {
        if (i == (size - 1)) {
            total += (str[i] & 0xff) >> e;
        } else {
            total += str[i] << (size - 1 - i) * (8 - e);
        }
    }

    total += (str[size - 1] & 0x03) * 0.25f;
    return total;
}

float smc_value_to_float(SMCVal_t val)
{
    float fval = -1.0f;

    if (val.dataSize > 0) {
        if (strcmp(val.dataType, DATATYPE_FLT) == 0 && val.dataSize == 4) {
            memcpy(&fval, val.bytes, sizeof(float));
        } else if (strcmp(val.dataType, DATATYPE_FPE2) == 0 && val.dataSize == 2) {
            fval = smc_fpe_to_float(val.bytes, val.dataSize, 2);
        } else if (strcmp(val.dataType, DATATYPE_UINT16) == 0 && val.dataSize == 2) {
            fval = (float)smc_key_to_uint32((char *)val.bytes, val.dataSize, 10);
        } else if (strcmp(val.dataType, DATATYPE_UINT8) == 0 && val.dataSize == 1) {
            fval = (float)smc_key_to_uint32((char *)val.bytes, val.dataSize, 10);
        }
    }

    return fval;
}

static kern_return_t SMCCall(int index, SMCKeyData_t *input, SMCKeyData_t *output, io_connect_t conn)
{
    size_t input_size = sizeof(SMCKeyData_t);
    size_t output_size = sizeof(SMCKeyData_t);
    return IOConnectCallStructMethod(conn, index, input, input_size, output, &output_size);
}

kern_return_t SMCOpen(io_connect_t *conn)
{
    kern_return_t result;
    mach_port_t master_port;
    io_iterator_t iterator;
    io_object_t device;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    IOMasterPort(MACH_PORT_NULL, &master_port);
#pragma clang diagnostic pop

    CFMutableDictionaryRef matching = IOServiceMatching("AppleSMC");
    result = IOServiceGetMatchingServices(master_port, matching, &iterator);
    if (result != kIOReturnSuccess) {
        fprintf(stderr, "Error: IOServiceGetMatchingServices() = %08x\n", result);
        return result;
    }

    device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0) {
        fprintf(stderr, "Error: no AppleSMC service found\n");
        return kIOReturnNotFound;
    }

    result = IOServiceOpen(device, mach_task_self(), 0, conn);
    IOObjectRelease(device);
    if (result != kIOReturnSuccess) {
        fprintf(stderr, "Error: IOServiceOpen() = %08x\n", result);
    }

    return result;
}

kern_return_t SMCClose(io_connect_t conn)
{
    return IOServiceClose(conn);
}

kern_return_t SMCGetKeyInfo(UInt32 key, SMCKeyData_keyInfo_t *keyInfo, io_connect_t conn)
{
    SMCKeyData_t input;
    SMCKeyData_t output;
    kern_return_t result;

    memset(&input, 0, sizeof(SMCKeyData_t));
    memset(&output, 0, sizeof(SMCKeyData_t));

    input.key = key;
    input.data8 = SMC_CMD_READ_KEYINFO;

    result = SMCCall(KERNEL_INDEX_SMC, &input, &output, conn);
    if (result == kIOReturnSuccess) {
        *keyInfo = output.keyInfo;
    }

    return result;
}

kern_return_t SMCReadKey(UInt32Char_t key, SMCVal_t *val, io_connect_t conn)
{
    SMCKeyData_t input;
    SMCKeyData_t output;
    kern_return_t result;

    memset(&input, 0, sizeof(SMCKeyData_t));
    memset(&output, 0, sizeof(SMCKeyData_t));
    memset(val, 0, sizeof(SMCVal_t));

    input.key = smc_key_to_uint32(key, 4, 16);
    sprintf(val->key, "%s", key);

    result = SMCGetKeyInfo(input.key, &output.keyInfo, conn);
    if (result != kIOReturnSuccess) {
        return result;
    }

    val->dataSize = output.keyInfo.dataSize;
    if (val->dataSize == 0 || val->dataSize > sizeof(val->bytes)) {
        return kIOReturnBadArgument;
    }
    smc_uint32_to_key(val->dataType, output.keyInfo.dataType);
    input.keyInfo.dataSize = val->dataSize;
    input.data8 = SMC_CMD_READ_BYTES;

    result = SMCCall(KERNEL_INDEX_SMC, &input, &output, conn);
    if (result != kIOReturnSuccess) {
        return result;
    }

    memcpy(val->bytes, output.bytes, sizeof(output.bytes));
    return kIOReturnSuccess;
}

kern_return_t SMCWriteKey(SMCVal_t writeVal, io_connect_t conn)
{
    kern_return_t result;
    SMCKeyData_t input;
    SMCKeyData_t output;
    SMCVal_t readVal;

    result = SMCReadKey(writeVal.key, &readVal, conn);
    if (result != kIOReturnSuccess) {
        fprintf(stderr, "Error: SMCReadKey failed for %s: %08x\n", writeVal.key, result);
        return result;
    }

    if (readVal.dataSize != writeVal.dataSize) {
        fprintf(stderr, "Error: dataSize mismatch for %s (read=%u, write=%u)\n",
                writeVal.key, readVal.dataSize, writeVal.dataSize);
        return kIOReturnError;
    }

    memset(&input, 0, sizeof(SMCKeyData_t));
    memset(&output, 0, sizeof(SMCKeyData_t));

    input.key = smc_key_to_uint32(writeVal.key, 4, 16);
    input.data8 = SMC_CMD_WRITE_BYTES;
    input.keyInfo.dataSize = writeVal.dataSize;
    memcpy(input.bytes, writeVal.bytes, sizeof(writeVal.bytes));

    result = SMCCall(KERNEL_INDEX_SMC, &input, &output, conn);
    if (result != kIOReturnSuccess) {
        fprintf(stderr, "Error: SMCCall write failed for %s: %08x\n", writeVal.key, result);
    }

    return result;
}

static int read_fan_count_key(io_connect_t conn)
{
    SMCVal_t val;
    if (SMCReadKey("FNum", &val, conn) != kIOReturnSuccess) {
        return 0;
    }
    return (int)smc_key_to_uint32((char *)val.bytes, val.dataSize, 10);
}

static int make_fan_key(char key[5], int fan_num, const char *suffix)
{
    int written;

    if (fan_num < 0 || fan_num > 9 || suffix == NULL || strlen(suffix) != 2) {
        return 0;
    }

    written = snprintf(key, 5, "F%d%s", fan_num, suffix);
    return written == 4;
}

int fans_detect_count(io_connect_t conn)
{
    int count = read_fan_count_key(conn);
    if (count > 0) {
        return count;
    }

    for (int i = 0; i < 8; i++) {
        char key[5];
        SMCVal_t val;
        if (!make_fan_key(key, i, "Ac")) {
            break;
        }
        if (SMCReadKey(key, &val, conn) != kIOReturnSuccess) {
            break;
        }
        count++;
    }

    return count;
}

float fans_read_speed(int fan_num, io_connect_t conn)
{
    SMCVal_t val;
    char key[5];
    if (!make_fan_key(key, fan_num, "Ac")) {
        return -1;
    }

    if (SMCReadKey(key, &val, conn) != kIOReturnSuccess) {
        return -1;
    }

    return smc_value_to_float(val);
}

static kern_return_t set_fan_test_mode(int enabled, io_connect_t conn)
{
    SMCVal_t val;
    kern_return_t result = SMCReadKey("Ftst", &val, conn);
    if (result != kIOReturnSuccess || val.dataSize != 1) {
        return kIOReturnSuccess;
    }

    val.bytes[0] = enabled ? 1 : 0;
    sprintf(val.key, "%s", "Ftst");
    result = SMCWriteKey(val, conn);
    if (result != kIOReturnSuccess) {
        fprintf(stderr, "Warning: failed to write Ftst=%d: %08x\n", enabled ? 1 : 0, result);
    }

    return result;
}

static kern_return_t write_force_bits_value(uint16_t bits, io_connect_t conn)
{
    SMCVal_t val;
    kern_return_t result = SMCReadKey("FS! ", &val, conn);
    if (result != kIOReturnSuccess || val.dataSize < 2) {
        return kIOReturnSuccess;
    }

    val.bytes[0] = (bits >> 8) & 0xFF;
    val.bytes[1] = bits & 0xFF;
    sprintf(val.key, "%s", "FS! ");

    result = SMCWriteKey(val, conn);
    if (result != kIOReturnSuccess) {
        fprintf(stderr, "Warning: failed to write FS! force bits: %08x\n", result);
    }

    return result;
}

static kern_return_t write_force_bits(int fan_count, int enabled, io_connect_t conn)
{
    uint16_t bits = enabled ? fans_force_bits(fan_count) : 0;
    return write_force_bits_value(bits, conn);
}

static kern_return_t set_fan_mode(int fan_num, int mode, io_connect_t conn)
{
    SMCVal_t val;
    char key[5];
    if (!make_fan_key(key, fan_num, "Md")) {
        return kIOReturnBadArgument;
    }

    kern_return_t result = SMCReadKey(key, &val, conn);
    if (result != kIOReturnSuccess) {
        return kIOReturnSuccess;
    }

    if (val.dataSize == 1) {
        val.bytes[0] = (UInt8)mode;
        sprintf(val.key, "%s", key);
        result = SMCWriteKey(val, conn);
    }

    return result;
}

static kern_return_t fans_set_speed_internal(int fan_num, int rpm, io_connect_t conn, int quiet)
{
    SMCVal_t val;
    char key[5];

    if (!fans_validate_rpm(rpm) || fan_num < 0 || fan_num > 9) {
        return kIOReturnBadArgument;
    }

    set_fan_mode(fan_num, 1, conn);

    if (!make_fan_key(key, fan_num, "Tg")) {
        return kIOReturnBadArgument;
    }
    kern_return_t result = SMCReadKey(key, &val, conn);
    if (result != kIOReturnSuccess) {
        if (!quiet) {
            fprintf(stderr, "Error: cannot read %s\n", key);
        }
        return result;
    }

    if (strcmp(val.dataType, DATATYPE_FLT) == 0 && val.dataSize == 4) {
        float fan_rpm = (float)rpm;
        memcpy(val.bytes, &fan_rpm, sizeof(float));
    } else if (strcmp(val.dataType, DATATYPE_FPE2) == 0 && val.dataSize == 2) {
        fans_encode_fpe2_rpm(rpm, val.bytes);
    } else {
        if (!quiet) {
            fprintf(stderr, "Error: unknown type %s for %s\n", val.dataType, key);
        }
        return kIOReturnError;
    }

    sprintf(val.key, "%s", key);
    result = SMCWriteKey(val, conn);
    if (result == kIOReturnSuccess) {
        kern_return_t mode_result = set_fan_mode(fan_num, 1, conn);
        if (mode_result != kIOReturnSuccess) {
            return mode_result;
        }
    }

    return result;
}

kern_return_t fans_set_speed(int fan_num, int rpm, io_connect_t conn)
{
    return fans_set_speed_internal(fan_num, rpm, conn, 0);
}

static int fan_target_matches(int fan_num, int rpm, io_connect_t conn)
{
    SMCVal_t val;
    char key[5];
    if (!make_fan_key(key, fan_num, "Tg")) {
        return 0;
    }

    if (SMCReadKey(key, &val, conn) != kIOReturnSuccess) {
        return 0;
    }

    return fans_target_matches(smc_value_to_float(val), rpm);
}

int fans_all_targets_match(int rpm, io_connect_t conn)
{
    int fan_count = fans_detect_count(conn);

    if (fan_count <= 0 || !fans_validate_rpm(rpm)) {
        return 0;
    }

    for (int i = 0; i < fan_count; i++) {
        if (!fan_target_matches(i, rpm, conn)) {
            return 0;
        }
    }

    return 1;
}

kern_return_t fans_set_auto(int fan_num, io_connect_t conn)
{
    return set_fan_mode(fan_num, 0, conn);
}

int fans_set_all(int rpm, io_connect_t conn)
{
    int fan_count = fans_detect_count(conn);
    if (fan_count <= 0) {
        fprintf(stderr, "Error: no fans detected\n");
        return 1;
    }

    if (!fans_validate_rpm(rpm)) {
        fprintf(stderr, "Error: RPM must be between %d and %d\n", FANS_MIN_RPM, FANS_MAX_RPM);
        return 1;
    }

    printf("Detected %d fan(s)\n", fan_count);
    printf("Setting all fans to %d RPM (forced mode)...\n", rpm);

    set_fan_test_mode(1, conn);
    write_force_bits(fan_count, 1, conn);

    int failed = 0;
    for (int i = 0; i < fan_count; i++) {
        kern_return_t result = kIOReturnError;
        int matched = 0;

        for (int attempt = 1; attempt <= 8; attempt++) {
            result = fans_set_speed(i, rpm, conn);
            if (result != kIOReturnSuccess) {
                break;
            }

            usleep(500000);
            if (fan_target_matches(i, rpm, conn)) {
                matched = 1;
                break;
            }
        }

        if (result == kIOReturnSuccess) {
            SMCVal_t val;
            char key[5];
            sprintf(key, "F%dTg", i);
            if (SMCReadKey(key, &val, conn) == kIOReturnSuccess) {
                printf("Fan %d target: %.0f RPM\n", i, smc_value_to_float(val));
            }
            printf("Fan %d current: %.0f RPM\n", i, fans_read_speed(i, conn));
            if (!matched) {
                fprintf(stderr, "Error: fan %d target did not read back near %d RPM\n", i, rpm);
                failed = 1;
            }
        } else {
            fprintf(stderr, "Error: failed to set fan %d: %08x\n", i, result);
            if (result == kIOReturnNotPrivileged) {
                fprintf(stderr, "Hint: run with sudo or install with make install\n");
            }
            failed = 1;
        }
    }

    if (failed) {
        fprintf(stderr, "Restoring automatic mode after failed set-all.\n");
        fans_set_all_auto(conn);
        return 1;
    }

    printf("Success: all detected fans requested at %d RPM\n", rpm);
    if (fans_saved_store_rpm(rpm) != 0) {
        fprintf(stderr, "Warning: could not save fan setting for wake restore\n");
    }
    return 0;
}

int fans_restore_quiet(int rpm, io_connect_t conn)
{
    int fan_count = fans_detect_count(conn);
    int failed = 0;

    if (fan_count <= 0) {
        return 1;
    }

    if (!fans_validate_rpm(rpm)) {
        return 1;
    }

    set_fan_test_mode(1, conn);
    write_force_bits(fan_count, 1, conn);

    for (int i = 0; i < fan_count; i++) {
        if (fan_target_matches(i, rpm, conn)) {
            continue;
        }

        if (fans_set_speed_internal(i, rpm, conn, 1) != kIOReturnSuccess) {
            failed = 1;
        }
    }

    return failed ? 1 : 0;
}

int fans_set_all_auto_quiet(io_connect_t conn)
{
    int fan_count = fans_detect_count(conn);
    int failed = 0;

    if (fan_count <= 0) {
        return 1;
    }

    write_force_bits(fan_count, 0, conn);

    for (int i = 0; i < fan_count; i++) {
        if (fans_set_auto(i, conn) != kIOReturnSuccess) {
            failed = 1;
        }
    }

    set_fan_test_mode(0, conn);
    return failed ? 1 : 0;
}

int fans_set_all_auto(io_connect_t conn)
{
    int fan_count = fans_detect_count(conn);
    if (fan_count <= 0) {
        fprintf(stderr, "Error: no fans detected\n");
        return 1;
    }

    printf("Restoring automatic mode for %d fan(s)...\n", fan_count);

    int failed = fans_set_all_auto_quiet(conn);
    if (!failed) {
        for (int i = 0; i < fan_count; i++) {
            printf("Fan %d: automatic mode\n", i);
        }
    } else {
        for (int i = 0; i < fan_count; i++) {
            fprintf(stderr, "Error: failed to restore fan %d to automatic mode\n", i);
        }
    }

    if (!failed && fans_saved_store_auto() != 0) {
        fprintf(stderr, "Warning: could not clear saved fan setting\n");
    }
    return failed ? 1 : 0;
}

void fans_print_info(io_connect_t conn)
{
    int fan_count = fans_detect_count(conn);
    printf("Total fans: %d\n", fan_count);

    for (int i = 0; i < fan_count; i++) {
        SMCVal_t val;
        char key[5];

        printf("\nFan #%d:\n", i);
        printf("  Current speed: %.0f RPM\n", fans_read_speed(i, conn));

        if (make_fan_key(key, i, "Mn") && SMCReadKey(key, &val, conn) == kIOReturnSuccess) {
            printf("  Min speed: %.0f RPM (type: %s)\n", smc_value_to_float(val), val.dataType);
        }

        if (make_fan_key(key, i, "Mx") && SMCReadKey(key, &val, conn) == kIOReturnSuccess) {
            printf("  Max speed: %.0f RPM\n", smc_value_to_float(val));
        }

        if (make_fan_key(key, i, "Tg") && SMCReadKey(key, &val, conn) == kIOReturnSuccess) {
            printf("  Target speed: %.0f RPM\n", smc_value_to_float(val));
        }
    }
}

int fans_read_key_command(const char *key_name, io_connect_t conn)
{
    SMCVal_t val;
    char key[5];
    fans_copy_smc_key(key_name, key);

    kern_return_t result = SMCReadKey(key, &val, conn);
    if (result != kIOReturnSuccess) {
        fprintf(stderr, "Error: cannot read key %s: %08x\n", key, result);
        return 1;
    }

    printf("Key: %s\n", key);
    printf("Type: %s\n", val.dataType);
    printf("Size: %u\n", val.dataSize);
    printf("Value: %.2f\n", smc_value_to_float(val));
    printf("Bytes: ");
    for (UInt32 i = 0; i < val.dataSize; i++) {
        printf("%02x ", (unsigned char)val.bytes[i]);
    }
    printf("\n");

    return 0;
}

void fans_usage(const char *prog)
{
    printf("mac-fans-cli\n");
    printf("Usage:\n");
    printf("  %s info                     Show fan information\n", prog);
    printf("  %s <RPM>                    Set all detected fans to RPM\n", prog);
    printf("  %s set-all <RPM>            Set all detected fans to RPM\n", prog);
    printf("  %s set <FAN#> <RPM>         Set one fan target speed\n", prog);
    printf("  %s reset                    Return all fans to automatic (default)\n", prog);
    printf("  %s auto-all                 Same as reset\n", prog);
    printf("  %s auto <FAN#>              Restore automatic mode for one fan\n", prog);
    printf("  %s read <KEY>               Read a raw SMC key\n", prog);
    printf("\nForced RPM from set-all is saved and re-applied after sleep (via make install).\n");
    printf("\nExamples:\n");
    printf("  %s 3500\n", prog);
    printf("  %s info\n", prog);
    printf("  %s auto-all\n", prog);
}
