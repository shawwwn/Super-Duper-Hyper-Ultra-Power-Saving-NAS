#include "gpio.h"

int get_gpio(int pin)
{
	int ret = 0;

	if (!gpio_is_valid(pin)) {
		ret = -1;
		goto out;
	}

	if (gpio_request(pin, "") != 0) {
		ret = -2;
		goto out;
	}

	ret = gpio_get_value(pin);

out:
	gpio_free(pin);
	return ret;
}

int set_gpio(int pin, int val)
{
	int ret = 0;

	if (!gpio_is_valid(pin)) {
		ret = -1;
		goto out;
	}

	if (gpio_request(pin, "") != 0) {
		ret = -2;
		goto out;
	}

	if (gpio_direction_output(pin, gpio_get_value(pin)) != 0) {
		ret = -3;
		goto out;
	}

	gpio_set_value(pin, val);
	ret = 0;

out:
	gpio_free(pin);
	return ret;
}
