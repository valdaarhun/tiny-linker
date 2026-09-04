#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "elf/elf.h"

static void copy_from_elf_symtab(Elf_symbol_t *elf_symbol, Symbol_t *symbol) {
    symbol->info = elf_symbol->st_info;
    symbol->other = elf_symbol->st_other;
    symbol->shndx = elf_symbol->st_shndx;
    symbol->value = elf_symbol->st_value;
    symbol->size = elf_symbol->st_size;
}

static int insert_symbol(Symbol_t **sl_head, Symbol_t **sl_tail, char *sym_name, Elf_symbol_t *elf_symbol, uint32_t num) {
    int fl = 0;
    Symbol_t *ptr = *sl_head;

    for(int i = 0; i < num; i++) {
        if (!strcmp(sym_name, ptr->name)) {
            if (elf_symbol->st_shndx == SHN_UNDEF) {
                // Ignore repetitive references
                return 0;
            }

            if (ptr->shndx != SHN_UNDEF) {
                // Symbol has been redefined, throw an error
                return -1;
            }

            fl = 1;
            break;
        }

        ptr = ptr->next;
    }

    if (fl) {
        copy_from_elf_symtab(elf_symbol, ptr);
        return 0;
    }

    // Append at end of list
    Symbol_t *temp = (Symbol_t *)malloc(sizeof(Symbol_t));
    memset((void *)temp, 0, sizeof(Symbol_t));

    copy_from_elf_symtab(elf_symbol, temp);
    temp->name = sym_name;

    if (!*sl_head) {
        *sl_head = temp;
    } else {
        (*sl_tail)->next = temp;
    }

    *sl_tail = temp;

    return 1;
}

void assign_section_vaddr(Elf_list *elves, Elf_exec_t *exec) {
    int page_size = getpagesize();
    Elf_info_t *elf = elves->head;

    exec->text_vaddr = START_ADDR;

    while(elf) {
        exec->text_size += elf->text.size;
        elf = elf->next;
    }

    /*
     * Do the same thing for other sections (e.g. ".data"). Ensure the section starts
     * at a page boundary.
     */
    exec->data_vaddr = exec->text_vaddr + ((exec->text_size + page_size - 1) / page_size) * page_size;
    elf = elves->head;

    while(elf) {
        exec->data_size += elf->data.size;
        elf = elf->next;
    }

    printf("text -> 0x%lx of len 0x%x\n", exec->text_vaddr, exec->text_size);
    printf("text -> 0x%lx of len 0x%x\n", exec->data_vaddr, exec->data_size);
}

int resolve_symbols(Elf_list *elves, Elf_exec_t *exec) {
    Elf_info_t *elf = elves->head;
    Symbol_t *sl_tail = NULL, *sptr;
    uint32_t num_symbols = 0;
    Elf_symbol_t *elf_symbols;

    while(elf) {
        int cnt = 0;
        elf_symbols = elf->symbols;

        for(int i = 1; i < elf->num_symbols; i++) {
            if ((elf_symbols[i].st_shndx >> 8) == SHN_SPECIAL)
                continue;

            unsigned char type = elf_symbols[i].st_info & 0xf;
            if (type == STT_SECTION || type == STT_FILE)
                continue;

            /*
             * This is a symbol that we need to keep a track of. Check
             * if the symbol has already been defined or referenced earlier.
             * If already defined, throw an error
             * If it has been referenced, resolve it.
             * If it's a new symbol, add it to the symbol table
             */
            int res = insert_symbol(&exec->symbols.head, &sl_tail, &elf->symstr[elf_symbols[i].st_name],
                                      &elf_symbols[i], num_symbols);

            printf("%s %s\n", &elf->symstr[elf_symbols[i].st_name], &elf->shstr[elf_symbols[i].st_shndx]);
            if (res < 0) {
                fprintf(stderr,
                        "Found multiple definitions of symbol '%s'\n",
                        &elf->symstr[elf_symbols[i].st_name]);

                return -1;
            }

            cnt += res;
        }

        num_symbols += cnt;
        elf = elf->next;
    }

    exec->symbols.num_symbols = num_symbols;
    sptr = exec->symbols.head;

#ifdef DEBUG
    printf("Num of symbols: %d\n", exec->symbols.num_symbols);
#endif

    while(sptr) {
        if (sptr->shndx == SHN_UNDEF) {
            fprintf(stderr, "Couldn't find definition for symbol '%s'\n", sptr->name);
            return -1;
        }

#ifdef DEBUG
        printf("---------------\n");
        printf("%s\n", sptr->name);
        printf("0x%x\n", sptr->shndx);
        printf("0x%lx\n", sptr->value);
#endif
        sptr = sptr->next;
    }

    return 0;
}
