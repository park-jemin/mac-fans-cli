#include "fans.h"
#include "fans_logic.h"
#include "smc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int open_or_fail(io_connect_t *conn)
{
    kern_return_t result = SMCOpen(conn);
    if (result != kIOReturnSuccess) {
        fprintf(stderr, "Error: cannot open SMC connection\n");
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    io_connect_t conn = 0;
    int status = 0;

    if (argc < 2) {
        fans_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];
    int rpm = 0;

    if (fans_parse_int(cmd, &rpm)) {
        if (open_or_fail(&conn) != 0) {
            return 1;
        }
        status = fans_set_all(rpm, conn);
        SMCClose(conn);
        return status;
    }

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        fans_usage(argv[0]);
        return 0;
    }

    if (open_or_fail(&conn) != 0) {
        return 1;
    }

    if (strcmp(cmd, "info") == 0) {
        fans_print_info(conn);
    } else if (strcmp(cmd, "set-all") == 0) {
        if (argc < 3 || !fans_parse_int(argv[2], &rpm)) {
            fprintf(stderr, "Error: set-all requires an integer RPM\n");
            status = 1;
        } else {
            status = fans_set_all(rpm, conn);
        }
    } else if (strcmp(cmd, "set") == 0) {
        int fan_num = 0;
        int fan_count = fans_detect_count(conn);
        if (argc < 4 || !fans_parse_int(argv[2], &fan_num) || !fans_parse_int(argv[3], &rpm)) {
            fprintf(stderr, "Error: set requires a fan number and integer RPM\n");
            status = 1;
        } else if (fan_count <= 0) {
            fprintf(stderr, "Error: no fans detected\n");
            status = 1;
        } else if (fan_num < 0 || fan_num >= fan_count) {
            fprintf(stderr, "Error: fan number must be between 0 and %d\n", fan_count - 1);
            status = 1;
        } else if (!fans_validate_rpm(rpm)) {
            fprintf(stderr, "Error: RPM must be between %d and %d\n", FANS_MIN_RPM, FANS_MAX_RPM);
            status = 1;
        } else {
            printf("Setting fan %d to %d RPM (forced mode)...\n", fan_num, rpm);
            kern_return_t result = fans_set_speed(fan_num, rpm, conn);
            if (result == kIOReturnSuccess) {
                printf("Success\n");
            } else {
                fprintf(stderr, "Error: failed to set fan %d: %08x\n", fan_num, result);
                status = 1;
            }
        }
    } else if (strcmp(cmd, "auto-all") == 0) {
        status = fans_set_all_auto(conn);
    } else if (strcmp(cmd, "auto") == 0) {
        int fan_num = 0;
        int fan_count = fans_detect_count(conn);
        if (argc < 3 || !fans_parse_int(argv[2], &fan_num)) {
            fprintf(stderr, "Error: auto requires a fan number\n");
            status = 1;
        } else if (fan_count <= 0) {
            fprintf(stderr, "Error: no fans detected\n");
            status = 1;
        } else if (fan_num < 0 || fan_num >= fan_count) {
            fprintf(stderr, "Error: fan number must be between 0 and %d\n", fan_count - 1);
            status = 1;
        } else {
            kern_return_t result = fans_set_auto(fan_num, conn);
            if (result == kIOReturnSuccess) {
                printf("Fan %d: automatic mode\n", fan_num);
            } else {
                fprintf(stderr, "Error: failed to restore fan %d: %08x\n", fan_num, result);
                status = 1;
            }
        }
    } else if (strcmp(cmd, "read") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: read requires an SMC key\n");
            status = 1;
        } else {
            status = fans_read_key_command(argv[2], conn);
        }
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        fans_usage(argv[0]);
        status = 1;
    }

    SMCClose(conn);
    return status;
}
