#ifndef HEADER_UTIL
#define HEADER_UTIL

typedef enum {ST_OFF, ST_ON, ST_BUSY} state_t;

extern char* mntscript;
extern char* mntpt;
extern char* uuid;
extern int gpio_pin;
extern state_t state;

int nas_try_poweron(void);
int nas_path_match_with_str(const char* pathname, const char* str);
int nas_path_match_with_fd(const char* pathname, int fd);
int nas_unmount(const char* pathname);
int nas_check_mnt(const char *pathname);
int file_exist(char* pathname);
int call_mountscript(void);
struct block_device *blkdev_get_by_mountpoint(char* pathname);
struct device *get_first_usb_device(struct block_device *bdev);
int remove_usb_device(struct device* dev);
char* get_fd_pathname(int fd, char* buf);
char* get_pwd_pathname(char* buf, int buflen);
int reset_pwd(void);
int fd_on_current_mnt(int fd);
int is_pwd_mounted(void);
char *join_path(const char *base, const char *relative, char *resolved);

#endif
