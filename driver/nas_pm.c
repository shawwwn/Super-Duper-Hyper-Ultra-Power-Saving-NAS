#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <linux/semaphore.h>
#include <asm/tlbflush.h>
#include <linux/string.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>

static void **my_sys_call_table = NULL;
#ifdef __aarch64__
static void **my_compat_sys_call_table = NULL;
#define __NR_compat_openat 322 // for arm64
#endif

static char* mnt_path;
static size_t mnt_path_len;

static int turn_on_nas(void) {
	return 0;
}

static inline int nas_path_match_with_str(const char* path) {
	return (strncmp(mnt_path, path, mnt_path_len) == 0);
}

static inline int nas_path_match_with_fd(int fd) {
	struct file* f;
	char buf[PATH_MAX];
	char* pwd;

	if (fd < 0)
		return false;

	f = fget_raw(fd);
	if (f == NULL)
		return false;

	// pwd = dentry_path_raw(current->fs->pwd.dentry, buf, PATH_MAX);
	pwd = d_absolute_path(&f->f_path, buf, PATH_MAX); // get full path
	if (pwd == NULL)
		return false;

	// printk(KERN_INFO "openat = %s\n", pwd);

	return nas_path_match_with_str(pwd);
}

asmlinkage long (*org_sys_openat)(int dfd, const char __user *filename, int flags, umode_t mode);
asmlinkage long my_sys_openat(int dfd, const char __user *filename, int flags, umode_t mode)
{
	if (*filename != '/') {
		// match after running openat()
		int fd = org_sys_openat(dfd, filename, flags, mode);
		if (nas_path_match_with_fd(fd))
			turn_on_nas();
		return fd;
	} else {
		// match before running openat()
		if (nas_path_match_with_str(filename)) 
			turn_on_nas();
	}

	return org_sys_openat(dfd, filename, flags, mode);
}

#ifdef __aarch64__
asmlinkage long (*org_compat_sys_openat)(int dfd, const char __user *filename, int flags, umode_t mode);
asmlinkage long my_compat_sys_openat(int dfd, const char __user *filename, int flags, umode_t mode)
{
	if (*filename != '/') {
		// match after running openat()
		int fd = org_compat_sys_openat(dfd, filename, flags, mode);
		if (nas_path_match_with_fd(fd))
			turn_on_nas();
		return fd;
	} else {
		// match before running openat()
		if (nas_path_match_with_str(filename)) 
			turn_on_nas();
	}

	return org_compat_sys_openat(dfd, filename, flags, mode);
}
#endif




/*
 * Get pte from virtual address
 * pgd -> pud -> pmd -> pte
 */
static inline pte_t* get_pte(struct mm_struct* mm_p, unsigned long addr)
{
	pgd_t *pgd;
	pte_t *pte;
	pud_t *pud;
	pmd_t *pmd;

	printk(KERN_INFO "addr=0x%lx\n", addr);
	printk(KERN_INFO "mm=0x%p\n", mm_p);

	pgd = pgd_offset(mm_p, addr);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		goto out;
	printk(KERN_INFO "pgd=0x%llx\n", pgd_val(*pgd));

	pud = pud_offset(pgd, addr);
	if (pud_none(*pud) || pud_bad(*pud))
		goto out;
	printk(KERN_INFO "pud=0x%llx\n", pud_val(*pud));

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		goto out;
	printk(KERN_INFO "pmd=0x%llx\n", pmd_val(*pmd));

	pte = pte_offset_map(pmd, addr);
	if (pte_none(*pte))
		goto out;
	printk(KERN_INFO "pte=0x%llx\n", pte_val(*pte));

	return pte;

	out:
		return NULL;
}

/* 
 * Enable write access to page memory
 */
static inline void pte_enable_write(pte_t *pte)
{
	*pte = set_pte_bit(*pte, __pgprot(PTE_WRITE)); // 52th bit
	*pte = clear_pte_bit(*pte, __pgprot(PTE_RDONLY)); // 8th bit

	flush_tlb_all();
}

/*
 * Disable write access to page memory
 */
static inline void pte_disable_write(pte_t *pte)
{
	*pte = clear_pte_bit(*pte, __pgprot(PTE_WRITE)); // 52th bit
	*pte = set_pte_bit(*pte, __pgprot(PTE_RDONLY)); // 8th bit

	flush_tlb_all();
}





static int __init init_func(void)
{
	pte_t* pte;
	struct mm_struct* init_mm_p = (struct mm_struct*)kallsyms_lookup_name("init_mm");

	printk(KERN_INFO "module 'test' start\n");
	mnt_path = "/media/usb2"; // TODO: pass in as parameters
	mnt_path_len = strlen(mnt_path);

	my_sys_call_table = (void *)kallsyms_lookup_name("sys_call_table");
	if (my_sys_call_table == NULL) {
		printk(KERN_ERR "Couldn't find sys_call_table\n");
		return -1;
	}
	printk(KERN_INFO "sys_call_table: %p\n", my_sys_call_table);
	org_sys_openat = (void *)(my_sys_call_table[__NR_openat]);
	printk(KERN_INFO "sys_openat: %p\n", org_sys_openat);

	#ifdef __aarch64__
	my_compat_sys_call_table = (void *)kallsyms_lookup_name("compat_sys_call_table");
	if (my_compat_sys_call_table == NULL) {
		printk(KERN_ERR "Couldn't find compat_sys_call_table\n");
		return -1;
	}
	printk(KERN_INFO "compat_sys_call_table: %p\n", my_compat_sys_call_table);
	org_compat_sys_openat=(void*)(my_compat_sys_call_table[__NR_compat_openat]);
	printk(KERN_INFO "compat_sys_openat: %p\n", org_compat_sys_openat);
	#endif

	pte = get_pte(init_mm_p, (unsigned long)&my_sys_call_table[__NR_openat]);
	pte_enable_write(pte);
	my_sys_call_table[__NR_openat] = &my_sys_openat;
	printk(KERN_INFO "replace sys_openat %p => %p\n", org_sys_openat, my_sys_openat);
	pte_disable_write(pte);

	#ifdef __aarch64__
	pte = get_pte(init_mm_p, (unsigned long)&my_compat_sys_call_table[__NR_compat_openat]);
	pte_enable_write(pte);
	// my_sys_call_table[__NR_compat_openat] = my_compat_sys_openat;
	printk(KERN_INFO "replace compat_sys_openat %p => %p\n", org_compat_sys_openat, my_compat_sys_openat);
	pte_disable_write(pte);
	#endif

	return 0;
}

static void __exit exit_func(void)
{
	struct mm_struct* init_mm_p;
	pte_t* pte;

	printk(KERN_INFO "Exiting hello module...\n");

	init_mm_p = (struct mm_struct*)kallsyms_lookup_name("init_mm");
	pte = get_pte(init_mm_p, (unsigned long)&my_sys_call_table[__NR_openat]);
	pte_enable_write(pte);
	my_sys_call_table[__NR_openat] = org_sys_openat;
	pte_disable_write(pte);

	#ifdef __aarch64__
	pte = get_pte(init_mm_p, (unsigned long)&my_compat_sys_call_table[__NR_compat_openat]);
	pte_enable_write(pte);
	my_compat_sys_call_table[__NR_compat_openat] = org_compat_sys_openat;
	pte_disable_write(pte);
	#endif
}

module_init(init_func);
module_exit(exit_func);

MODULE_AUTHOR("Shawwwn");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Super Duper Hyper Ultra Power Saving NAS Power Management Driver");
