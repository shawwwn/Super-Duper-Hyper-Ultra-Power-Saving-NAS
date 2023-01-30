#ifndef PATCH_H
#define PATCH_H

#include <linux/kprobes.h>

typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
volatile extern kallsyms_lookup_name_t my_kallsyms_lookup_name;

int init_kp(void);

#endif
