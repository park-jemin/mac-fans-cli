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
#include <time.h>
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

static const char *fans_saved_home_dir(void)
{
    struct passwd *pw;
    const char *home;
    uid_t uid = fans_owner_uid();

    home = getenv("HOME");
    if (home != NULL && home[0] != '\0') {
        return home;
    }

    pw = getpwuid(uid);
    if (pw == NULL || pw->pw_dir == NULL || pw->pw_dir[0] == '\0') {
        return NULL;
    }

    return pw->pw_dir;
}

static int fans_saved_chown_path(const char *path, int is_dir)
{
    struct passwd *pw;
    uid_t uid = fans_owner_uid();

    if (path == NULL) {
        return -1;
    }

    pw = getpwuid(uid);
    if (pw == NULL) {
        return -1;
    }

    if (chown(path, uid, pw->pw_gid) != 0) {
        return -1;
    }

    return chmod(path, is_dir ? 0700 : 0600);
}

static int fans_saved_path(char *buf, size_t buflen)
{
    const char *home = fans_saved_home_dir();

    if (buf == NULL || buflen == 0 || home == NULL) {
        return -1;
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
        fans_saved_chown_path(dir, 1);
        *p = '/';
    }

    if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
        return -1;
    }

    fans_saved_chown_path(dir, 1);
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
    if (saved->skip_wake_until > 0) {
        fprintf(fp, "skip_wake_until=%ld\n", saved->skip_wake_until);
    }

    fclose(fp);
    fans_saved_chown_path(path, 0);
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
    saved->skip_wake_until = 0;

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
        if (!fans_saved_parse_boot_line(line, saved)) {
            fans_saved_parse_skip_wake_line(line, saved);
        }
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
    saved.skip_wake_until = 0;
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
    saved.skip_wake_until = 0;
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

static int fans_run_restore_on_wake_internal(int force)
{
    io_connect_t conn = 0;
    fans_saved_t saved;
    long now_sec = 0;
    kern_return_t result;
    int status;

    if (fans_saved_peek_forced(&saved) != 1) {
        return 0;
    }

    now_sec = (long)time(NULL);
    if (!force && fans_saved_should_skip_wake(&saved, now_sec)) {
        saved.skip_wake_until = 0;
        fans_saved_write(&saved);
        return 0;
    }

    saved.skip_wake_until = 0;

    result = SMCOpen(&conn);
    if (result != kIOReturnSuccess) {
        return 1;
    }

    status = fans_restore_quiet(saved.rpm, conn);
    SMCClose(conn);

    if (status == 0 && !saved.has_boot) {
        long boot_sec = 0;
        long boot_usec = 0;
        if (fans_boottime_now(&boot_sec, &boot_usec) == 0) {
            fans_saved_record_boot(&saved, boot_sec, boot_usec);
        }
    }

    return status;
}

int fans_run_restore_on_wake_forced(void)
{
    return fans_run_restore_on_wake_internal(1);
}

int fans_run_restore_boot_session(void)
{
    io_connect_t conn = 0;
    fans_saved_t saved;
    long boot_sec = 0;
    long boot_usec = 0;
    kern_return_t result;
    int status;

    if (fans_saved_peek_forced(&saved) != 1) {
        return 0;
    }

    if (fans_boottime_now(&boot_sec, &boot_usec) != 0) {
        return 1;
    }

    if (fans_saved_boot_action(&saved, boot_sec, boot_usec) != 1) {
        return 0;
    }

    result = SMCOpen(&conn);
    if (result != kIOReturnSuccess) {
        return 1;
    }

    status = fans_set_all_auto_quiet(conn);
    if (status == 0) {
        saved.skip_wake_until = (long)time(NULL) + FANS_BOOT_SKIP_WAKE_SEC;
        fans_saved_record_boot(&saved, boot_sec, boot_usec);
    }

    SMCClose(conn);
    return status;
}

static void fans_restore_log(const char *message)
{
    FILE *fp;
    char path[512];
    const char *home = fans_saved_home_dir();

    if (message == NULL || home == NULL) {
        return;
    }

    if (snprintf(path, sizeof(path), "%s/%s/restore.log", home, FANS_SAVED_DIR) >= (int)sizeof(path)) {
        return;
    }

    fans_saved_ensure_dir();
    fp = fopen(path, "a");
    if (fp == NULL) {
        return;
    }

    fprintf(fp, "%ld %s\n", (long)time(NULL), message);
    fclose(fp);
    fans_saved_chown_path(path, 0);
}

static int fans_restore_verify_targets(int rpm)
{
    io_connect_t conn = 0;
    kern_return_t result;
    int ok = 0;

    result = SMCOpen(&conn);
    if (result != kIOReturnSuccess) {
        return 0;
    }

    ok = fans_all_targets_match(rpm, conn);
    SMCClose(conn);
    return ok;
}

static int fans_restore_apply_once(int rpm)
{
    io_connect_t conn = 0;
    kern_return_t result;
    int status = 1;

    for (int attempt = 1; attempt <= 8; attempt++) {
        result = SMCOpen(&conn);
        if (result != kIOReturnSuccess) {
            sleep(1);
            continue;
        }

        status = fans_restore_quiet(rpm, conn);
        SMCClose(conn);
        conn = 0;

        if (status == 0 && fans_restore_verify_targets(rpm)) {
            return 0;
        }

        sleep(1);
    }

    return 1;
}

static int fans_restore_wake_persist(int rpm)
{
    char log_line[96];
    int any_apply_ok = 0;

    snprintf(log_line, sizeof(log_line), "launchd: wake sequence start rpm=%d", rpm);
    fans_restore_log(log_line);

    sleep(FANS_WAKE_INITIAL_DELAY_SEC);

    for (int round = 0; round < FANS_WAKE_RETRY_ROUNDS; round++) {
        if (round > 0) {
            sleep(FANS_WAKE_RETRY_INTERVAL_SEC);
        }

        if (fans_restore_apply_once(rpm) != 0) {
            snprintf(log_line, sizeof(log_line), "launchd: wake round %d apply failed", round);
            fans_restore_log(log_line);
            continue;
        }

        any_apply_ok = 1;
        snprintf(log_line, sizeof(log_line), "launchd: wake round %d apply ok", round);
        fans_restore_log(log_line);
    }

    if (!any_apply_ok) {
        return 1;
    }

    sleep(FANS_WAKE_STABILITY_SEC);

    if (!fans_restore_verify_targets(rpm)) {
        fans_restore_log("launchd: wake stability check failed");
        return 1;
    }

    snprintf(log_line, sizeof(log_line), "launchd: wake restore ok rpm=%d", rpm);
    fans_restore_log(log_line);
    return 0;
}

int fans_run_restore_launchd(void)
{
    fans_saved_t saved;
    long boot_sec = 0;
    long boot_usec = 0;
    long now_sec = 0;
    char log_line[96];

    if (fans_saved_peek_forced(&saved) != 1) {
        fans_restore_log("launchd: no forced rpm saved");
        return 0;
    }

    now_sec = (long)time(NULL);

    if (fans_boottime_now(&boot_sec, &boot_usec) != 0) {
        fans_restore_log("launchd: cannot read boot time");
        return 1;
    }

    if (fans_saved_boot_action(&saved, boot_sec, boot_usec)) {
        io_connect_t conn = 0;
        kern_return_t result = SMCOpen(&conn);
        int status = 1;

        if (result == kIOReturnSuccess) {
            status = fans_set_all_auto_quiet(conn);
            SMCClose(conn);
        }

        if (status == 0) {
            saved.skip_wake_until = now_sec + FANS_BOOT_SKIP_WAKE_SEC;
            fans_saved_record_boot(&saved, boot_sec, boot_usec);
            snprintf(log_line, sizeof(log_line), "launchd: boot auto ok rpm=%d", saved.rpm);
        } else {
            snprintf(log_line, sizeof(log_line), "launchd: boot auto failed rpm=%d", saved.rpm);
        }

        fans_restore_log(log_line);
        return status;
    }

    if (fans_saved_should_skip_wake(&saved, now_sec)) {
        saved.skip_wake_until = 0;
        fans_saved_write(&saved);
        fans_restore_log("launchd: skipped duplicate wake after boot");
        return 0;
    }

    saved.skip_wake_until = 0;
    fans_saved_write(&saved);

    if (fans_restore_wake_persist(saved.rpm) == 0) {
        if (!saved.has_boot) {
            fans_saved_record_boot(&saved, boot_sec, boot_usec);
        }
        return 0;
    }

    snprintf(log_line, sizeof(log_line), "launchd: wake restore failed rpm=%d", saved.rpm);
    fans_restore_log(log_line);
    return 1;
}

int fans_run_restore(void)
{
    if (fans_run_restore_boot_session() != 0) {
        return 1;
    }
    return fans_run_restore_on_wake_forced();
}
