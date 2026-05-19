#ifndef FANS_H
#define FANS_H

#include <IOKit/IOKitLib.h>

int fans_detect_count(io_connect_t conn);
float fans_read_speed(int fan_num, io_connect_t conn);
kern_return_t fans_set_speed(int fan_num, int rpm, io_connect_t conn);
kern_return_t fans_set_auto(int fan_num, io_connect_t conn);
int fans_set_all(int rpm, io_connect_t conn);
int fans_set_all_auto(io_connect_t conn);
void fans_print_info(io_connect_t conn);
int fans_read_key_command(const char *key_name, io_connect_t conn);
void fans_usage(const char *prog);

#endif
