#ifndef HEADER_UTIL
#define HEADER_UTIL

extern char* mntpt;
extern char* uuid;

int nas_try_poweron(void);
int nas_path_match_with_str(const char* pathname, const char* str);
int nas_path_match_with_fd(const char* pathname, int fd);
int nas_unmount(const char* pathname);
int nas_check_mnt(const char *pathname);
int call_mountscript(void);
struct block_device *blkdev_get_by_mountpoint(char* pathname);
struct device *get_first_usb_device(struct block_device *bdev);
int remove_usb_device(struct device* dev);

#endif
