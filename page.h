#ifndef HEADER_PAGE
#define HEADER_PAGE

#include <linux/pgtable.h>
#include <asm/tlbflush.h>

pte_t* get_pte(struct mm_struct* mm_p, unsigned long addr);
void pte_enable_write(pte_t *pte);
void pte_disable_write(pte_t *pte);

#endif
