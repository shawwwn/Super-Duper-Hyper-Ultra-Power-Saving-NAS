#ifndef HEADER_GPIO
#define HEADER_GPIO
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>

int get_gpio(int pin);
int set_gpio(int pin, int val);

#endif
