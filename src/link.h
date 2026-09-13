#ifndef LINK_H
#define LINK_H

#include "elf/elf.h"

void assign_vaddr(Elf_list *, Elf_exec_t *);
int create_symtab(Elf_list *, Elf_exec_t *);
int relocate_symbols(Elf_list *, Elf_exec_t *);

#endif
