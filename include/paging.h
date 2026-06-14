/* naos — B8: paging (virtual memory). See docs/howto/08-paging.md. */
#ifndef NAOS_PAGING_H
#define NAOS_PAGING_H

/* Build a page directory (identity-map the low 4 MB + a higher-half window at
 * 0xC0000000 → 0), load CR3, and turn on paging (CR0.PG). */
void paging_init(void);

#endif /* NAOS_PAGING_H */
