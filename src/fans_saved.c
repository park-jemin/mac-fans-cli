#include "fans.h"
#include "fans_logic.h"
#include "smc.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <unistd.h>

#define FANS_SAVED_DIR ".config/mac-fans-cli"
#define FANS_SAVED_FILE "state"

static uid_t fans_owner_uid(void)
{
    uid_t uid = getuid();
    if (geteuid() == 0 && uid != 0) {
        return uid;
    }
    return uid;
}

static int fans_boottime_now(long *boot_sec, long *boot_usec)
{
    struct timeval boottime;
    size_t size = sizeof(boottime);

    if (boot_sec == NULL || boot_usec == NULL) {
        return -1;
    }

    if (sysctlbyname("kern.boottime", &boottime, &size, NULL, 0) != 0) {
        return -1;
    }

    *boot_sec = boottime.tv_sec;
    *boot_usec = (long)boottime.tv_usec;
    return 0;
}

static int fans_saved_path(char *buf, size_t buflen)
{
    struct passwd *pw;
    const char *home;
    uid_t uid = fans_owner_uid();

    if (buf == NULL || buflen == 0) {
        return -1;
    }

    home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        pw = getpwuid(uid);
        if (pw == NULL || pw->pw_dir == NULL || pw->pw_dir[0] == '\0') {
            return -1;
        }
        home = pw->pw_dir;
    }

    if (snprintf(buf, buflen, "%s/%s/%s", home, FANS_SAVED_DIR, FANS_SAVED_FILE) >= (int)buflen) {
        return -1;
    }

    return 0;
}

static int fans_saved_ensure_dir(void)
{
    char path[512];
    char dir[512];
    char *slash;

    if (fans_saved_path(path, sizeof(path)) != 0) {
        return -1;
    }

    strncpy(dir, path, sizeof(dir));
    dir[sizeof(dir) - 1] = '\0';
    slash = strrchr(dir, '/');
    if (slash == NULL) {
        return -1;
    }
    *slash = '\0';

    for (char *p = dir + 1; *p != '\0'; p++) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
            return -1;
        }
        *p = '/';
    }

    if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

static int fans_saved_write(const fans_saved_t *saved)
{
    FILE *fp;
    char path[512];
    char rpm_line[32];
    char boot_line[48];

    if (saved == NULL || !fans_saved_format_line(saved, rpm_line, sizeof(rpm_line))) {
        return -1;
    }

    if (fans_saved_path(path, sizeof(path)) != 0 || fans_saved_ensure_dir() != 0) {
        return -1;
    }

    fp = fopen(path, "w");
    if (fp == NULL) {
        return -1;
    }

    fputs(rpm_line, fp);
    if (saved->has_boot && fans_saved_format_boot_line(saved, boot_line, sizeof(boot_line))) {
        fputs(boot_line, fp);
    }

    fclose(fp);
    return 0;
}

static int fans_saved_read(fans_saved_t *saved)
{
    FILE *fp;
    char path[512];
    char line[64];

    if (saved == NULL) {
        return -1;
    }

    saved->rpm = FANS_SAVED_AUTO;
    saved->has_boot = 0;
    saved->boot_sec = 0;
    saved->boot_usec = 0;

    if (fans_saved_path(path, sizeof(path)) != 0) {
        return -1;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }

    if (fgets(line, sizeof(line), fp) == NULL || !fans_saved_parse_line(line, saved)) {
        fclose(fp);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        fans_saved_parse_boot_line(line, saved);
    }

    fclose(fp);
    return 0;
}

static int fans_saved_with_boottime(fans_saved_t *saved)
{
    if (saved == NULL) {
        return -1;
    }

    if (fans_boottime_now(&saved->boot_sec, &saved->boot_usec) != 0) {
        return -1;
    }

    saved->has_boot = 1;
    return 0;
}

int fans_saved_store_rpm(int rpm)
{
    fans_saved_t saved;

    if (!fans_validate_rpm(rpm)) {
        return -1;
    }

    saved.rpm = rpm;
    if (fans_saved_with_boottime(&saved) != 0) {
        saved.has_boot = 0;
    }

    return fans_saved_write(&saved);
}

int fans_saved_store_auto(void)
{
    fans_saved_t saved;

    saved.rpm = FANS_SAVED_AUTO;
    saved.has_boot = 0;
    saved.boot_sec = 0;
    saved.boot_usec = 0;
    return fans_saved_write(&saved);
}

static int fans_saved_peek_forced(fans_saved_t *saved_out)
{
    if (saved_out == NULL) {
        return -1;
    }

    if (fans_saved_read(saved_out) != 0) {
        return 0;
    }

    if (saved_out->rpm == FANS_SAVED_AUTO || !fans_validate_rpm(saved_out->rpm)) {
        return 0;
    }

    return 1;
}

static int fans_saved_record_boot(fans_saved_t *saved, long boot_sec, long boot_usec)
{
    saved->has_boot = 1;
    saved->boot_sec = boot_sec;
    saved->boot_usec = boot_usec;
    return fans_saved_write(saved);
}

int fans_run_restore_wake(void)
{
    io_connect_t conn = 0;
    fans_saved_t saved;
    kern_return_t result;
    int status;

    if (fans_saved_peek_forced(&saved) != 1) {
        return 0;
    }

    result = SMCOpen(&conn);
    if (result != kIOReturnSuccess) {
        return 1;
    }

    status = fans_restore_quiet(saved.rpm, conn);

    if (status == 0) {
        long boot_sec = 0;
        long boot_usec = 0;
        if (!saved.has_boot && fans_boottime_now(&boot_sec, &boot_usec) == 0) {
            fans_saved_record_boot(&saved, boot_sec, boot_usec);
        }
    }

    SMCClose(conn);
    return status;
}

int fans_run_restore_boot(void)
{
    io_connect_t conn = 0;
    fans_saved_t saved;
    long boot_sec = 0;
    long boot_usec = 0;
    int action;
    kern_return_t result;
    int status;

    if (fans_saved_peek_forced(&saved) != 1) {
        return 0;
    }

    if (fans_boottime_now(&boot_sec, &boot_usec) != 0) {
        return 1;
    }

    action = fans_saved_boot_action(&saved, boot_sec, boot_usec);
    if (action == 0) {
        return 0;
    }

    result = SMCOpen(&conn);
    if (result != kIOReturnSuccess) {
        return 1;
    }

    status = fans_set_all_auto_quiet(conn);
    if (status == 0) {
        fans_saved_record_boot(&saved, boot_sec, boot_usec);
    }

    SMCClose(conn);
    return status;
}
