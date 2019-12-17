#include "page.h"

/*
 * Get pte from virtual address
 * pgd -> pud -> pmd -> pte
 */
pte_t* get_pte(struct mm_struct* mm_p, unsigned long addr)
{
	pgd_t *pgd;
	pte_t *pte;
	pud_t *pud;
	pmd_t *pmd;

	// printk(KERN_INFO "addr=0x%lx\n", addr);
	// printk(KERN_INFO "mm=0x%p\n", mm_p);

	pgd = pgd_offset(mm_p, addr);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		goto out;
	// printk(KERN_INFO "pgd=0x%llx\n", pgd_val(*pgd));

	pud = pud_offset(pgd, addr);
	if (pud_none(*pud) || pud_bad(*pud))
		goto out;
	// printk(KERN_INFO "pud=0x%llx\n", pud_val(*pud));

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		goto out;
	// printk(KERN_INFO "pmd=0x%llx\n", pmd_val(*pmd));

	pte = pte_offset_map(pmd, addr);
	if (pte_none(*pte))
		goto out;
	// printk(KERN_INFO "pte=0x%llx\n", pte_val(*pte));

	return pte;

	out:
		return NULL;
}

/* 
 * Enable write access to page memory
 */
void pte_enable_write(pte_t *pte)
{
	*pte = set_pte_bit(*pte, __pgprot(PTE_WRITE)); // 52th bit
	*pte = clear_pte_bit(*pte, __pgprot(PTE_RDONLY)); // 8th bit

	flush_tlb_all();
}

/*
 * Disable write access to page memory
 */
void pte_disable_write(pte_t *pte)
{
	*pte = clear_pte_bit(*pte, __pgprot(PTE_WRITE)); // 52th bit
	*pte = set_pte_bit(*pte, __pgprot(PTE_RDONLY)); // 8th bit

	flush_tlb_all();
}

