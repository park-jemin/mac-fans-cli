#include "fans_logic.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_parse_auto(void)
{
    fans_saved_t saved = {.rpm = 0};

    assert(fans_saved_parse_line("auto", &saved));
    assert(saved.rpm == FANS_SAVED_AUTO);
    assert(fans_saved_parse_line(" auto \n", &saved));
    assert(saved.rpm == FANS_SAVED_AUTO);
}

static void test_parse_rpm(void)
{
    fans_saved_t saved = {.rpm = FANS_SAVED_AUTO};

    assert(fans_saved_parse_line("3500", &saved));
    assert(saved.rpm == 3500);
    assert(fans_saved_parse_line(" 500 ", &saved));
    assert(saved.rpm == 500);
    assert(fans_saved_parse_line("8000", &saved));
    assert(saved.rpm == 8000);
}

static void test_parse_invalid(void)
{
    fans_saved_t saved = {.rpm = 3500};

    assert(!fans_saved_parse_line("", &saved));
    assert(!fans_saved_parse_line("   ", &saved));
    assert(!fans_saved_parse_line("499", &saved));
    assert(!fans_saved_parse_line("8001", &saved));
    assert(!fans_saved_parse_line("fast", &saved));
    assert(!fans_saved_parse_line("3500rpm", &saved));
    assert(!fans_saved_parse_line(NULL, &saved));
    assert(!fans_saved_parse_line("3500", NULL));
}

static void test_format_roundtrip(void)
{
    fans_saved_t in = {.rpm = 4200};
    fans_saved_t out = {.rpm = FANS_SAVED_AUTO};
    char line[32];

    assert(fans_saved_format_line(&in, line, sizeof(line)));
    assert(strcmp(line, "4200\n") == 0);
    assert(fans_saved_parse_line(line, &out));
    assert(out.rpm == 4200);

    in.rpm = FANS_SAVED_AUTO;
    assert(fans_saved_format_line(&in, line, sizeof(line)));
    assert(strcmp(line, "auto\n") == 0);
    assert(fans_saved_parse_line(line, &out));
    assert(out.rpm == FANS_SAVED_AUTO);
}

static void test_format_invalid(void)
{
    fans_saved_t saved = {.rpm = 100};
    char line[8];

    assert(!fans_saved_format_line(NULL, line, sizeof(line)));
    assert(!fans_saved_format_line(&saved, NULL, 8));
    assert(!fans_saved_format_line(&saved, line, 0));
    assert(!fans_saved_format_line(&saved, line, sizeof(line)));
}

static void test_parse_boot_line(void)
{
    fans_saved_t saved = {.rpm = 3500};

    assert(fans_saved_parse_boot_line("boot 1710000000 123456", &saved));
    assert(saved.has_boot);
    assert(saved.boot_sec == 1710000000);
    assert(saved.boot_usec == 123456);
    assert(!fans_saved_parse_boot_line("boot 1", &saved));
    assert(!fans_saved_parse_boot_line("rpm 1 2", &saved));
}

static void test_boot_changed(void)
{
    fans_saved_t saved = {
        .rpm = 3500,
        .has_boot = 1,
        .boot_sec = 100,
        .boot_usec = 200,
    };

    assert(!fans_saved_boot_changed(&saved, 100, 200));
    assert(fans_saved_boot_changed(&saved, 101, 200));
    assert(fans_saved_boot_changed(&saved, 100, 201));
    assert(!fans_saved_boot_changed(&saved, 100, 200));

    saved.has_boot = 0;
    assert(!fans_saved_boot_changed(&saved, 100, 200));
}

static void test_parse_skip_wake(void)
{
    fans_saved_t saved = {.rpm = 3500};

    assert(fans_saved_parse_skip_wake_line("skip_wake=1", &saved));
    assert(saved.skip_wake_until == 1);
    assert(fans_saved_parse_skip_wake_line("skip_wake_until=9999999999", &saved));
    assert(saved.skip_wake_until == 9999999999);
    assert(fans_saved_should_skip_wake(&saved, 100));
    assert(!fans_saved_should_skip_wake(&saved, 10000000000L));
    assert(!fans_saved_parse_skip_wake_line("skip_wake=0", &saved));
    assert(!fans_saved_parse_skip_wake_line("boot 1 2", &saved));
}

static void test_boot_action(void)
{
    fans_saved_t saved = {
        .rpm = 3500,
        .has_boot = 1,
        .boot_sec = 100,
        .boot_usec = 200,
    };

    assert(fans_saved_boot_action(&saved, 101, 200) == 1);
    assert(fans_saved_boot_action(&saved, 100, 200) == 0);
    assert(fans_saved_boot_action(&saved, 100, 201) == 1);

    saved.has_boot = 0;
    assert(fans_saved_boot_action(&saved, 100, 200) == 1);

    saved.rpm = FANS_SAVED_AUTO;
    assert(fans_saved_boot_action(&saved, 101, 200) == 0);
}

static void test_format_boot_line(void)
{
    fans_saved_t saved = {
        .rpm = 3500,
        .has_boot = 1,
        .boot_sec = 99,
        .boot_usec = 1,
    };
    char line[48];

    assert(fans_saved_format_boot_line(&saved, line, sizeof(line)));
    assert(strcmp(line, "boot 99 1\n") == 0);

    saved.has_boot = 0;
    assert(!fans_saved_format_boot_line(&saved, line, sizeof(line)));
}

int main(void)
{
    test_parse_auto();
    test_parse_rpm();
    test_parse_invalid();
    test_format_roundtrip();
    test_format_invalid();
    test_parse_boot_line();
    test_boot_changed();
    test_parse_skip_wake();
    test_boot_action();
    test_format_boot_line();

    printf("fans_saved tests passed\n");
    return 0;
}
