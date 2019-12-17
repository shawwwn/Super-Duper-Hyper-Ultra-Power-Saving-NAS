#ifndef HEADER_UTIL
#define HEADER_UTIL

#include <linux/kmod.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/kallsyms.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <asm/uaccess.h>
#include <asm/errno.h>

extern char* mnt_path;

int nas_poweron(void);
int nas_path_match_with_str(const char* pathname, const char* str);
int nas_path_match_with_fd(const char* pathname, int fd);
int nas_unmount(const char* pathname);
int nas_check_mnt(const char *pathname);
int call_mountscript(void);

#endif
