#ifndef LINK_H
#define LINK_H

#include "elf/elf.h"

void assign_section_vaddr(Elf_list *, Elf_exec_t *);
int resolve_symbols(Elf_list *, Elf_exec_t *);

#endif
