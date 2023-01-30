#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>

#include "patch.h"

volatile kallsyms_lookup_name_t my_kallsyms_lookup_name;

int init_kp() {
	static struct kprobe kp = {
		.symbol_name = "kallsyms_lookup_name"
	};
	register_kprobe(&kp);
	my_kallsyms_lookup_name = (kallsyms_lookup_name_t) kp.addr;
	printk(KERN_INFO "kallsyms_lookup_name = 0x%px\n", kp.addr);
	unregister_kprobe(&kp);
	return 0;
}

