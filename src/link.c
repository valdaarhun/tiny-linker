#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "elf/elf.h"

/*
 * QUESTIONS:
 * ----------
 * 1. Do we need shndx in our final symbol table?
 * 2. If yes, do we store its name instead of file-specific index?
 */

static void copy_from_elf_symtab(Elf_symbol_t *elf_symbol, Symbol_t *symbol) {
    symbol->info = elf_symbol->st_info;
    symbol->other = elf_symbol->st_other;
    symbol->shndx = elf_symbol->st_shndx;
    symbol->value = elf_symbol->st_value;
    symbol->size = elf_symbol->st_size;
}

static int insert_symbol(uint16_t fndx, Symbol_t **sl_head, Symbol_t **sl_tail, Elf_symbol_t *elf_symbol, uint32_t num) {
    int fl = 0;
    Symbol_t *ptr = *sl_head;
    char *sym_name = elf_symbol->st_name;

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
        ptr->fndx = fndx;
        return 0;
    }

    // Append at end of list
    Symbol_t *symbol = (Symbol_t *)malloc(sizeof(Symbol_t));
    memset((void *)symbol, 0, sizeof(Symbol_t));

    copy_from_elf_symtab(elf_symbol, symbol);
    symbol->name = sym_name;
    symbol->fndx = fndx;

    if (!*sl_head) {
        *sl_head = symbol;
    } else {
        (*sl_tail)->next = symbol;
    }

    *sl_tail = symbol;

    return 1;
}

int resolve_symbols(Elf_list *elves, Elf_exec_t *exec) {
    Elf_info_t *elf = elves->head;
    Symbol_t *sl_tail = NULL, *sptr;
    uint32_t num_symbols = 0;
    char *symbol_name;
    Elf_symbol_t *elf_symbols;

    for(int fndx = 0; fndx < elves->num_files; fndx++) {
        int cnt = 0;
        elf_symbols = elf->symbols;

        for(int i = 1; i < elf->num_symbols; i++) {
            if ((elf_symbols[i].st_shndx >> 8) == SHN_SPECIAL)
                continue;

            unsigned char type = elf_symbols[i].st_info & 0xf;
            if (type == STT_FILE)
                continue;

            /*
             * This is a symbol that we need to keep a track of. Check
             * if the symbol has already been defined or referenced earlier.
             * If already defined, throw an error
             * If it has been referenced, resolve it.
             * If it's a new symbol, add it to the symbol table
             */
            int res = insert_symbol(fndx, &exec->symbols.head, &sl_tail, &elf_symbols[i], num_symbols);

            if (res < 0) {
                fprintf(stderr, "Found multiple definitions of symbol '%s'\n", elf_symbols[i].st_name);

                return -1;
            }

            cnt += res; // res will be 0 or 1
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
        printf("===============\n");
        printf("Symbol name: %s\n", sptr->name);
        printf("Associated section: 0x%x\n", sptr->shndx);
        printf("Symbol value: 0x%lx\n", sptr->value);
#endif
        sptr = sptr->next;
    }

#ifdef DEBUG
    printf("===============\n");
#endif

    return 0;
}

void assign_vaddr(Elf_list *elves, Elf_exec_t *exec) {
    int page_size = getpagesize();
    Elf_info_t *elf = elves->head;
    Symbol_t *symbols = exec->symbols.head;
    uint32_t *shndx = (uint32_t *)calloc(elves->num_files, sizeof(uint32_t));
    uint64_t *offset = (uint64_t *)calloc(elves->num_files, sizeof(uint64_t));

    memset((void *)shndx, 0, elves->num_files * sizeof(uint32_t));
    memset((void *)offset, 0, elves->num_files * sizeof(uint64_t));

    exec->text_vaddr = START_ADDR;

    for(int i = 0; i < elves->num_files; i++) {
        shndx[i] = elf->text.index;
        offset[i] = exec->text_size;
        // printf("%d %x\n", shndx[i], offset[i]);
        exec->text_size += elf->text.size;
        elf = elf->next;
    }

    /*
     * The value stored in Symbol_t::value is the offset of the symbol in
     * that specific file's section. We need to convert this value to the
     * offset of the symbol from the beginning of the consolidate section
     * as it would appear in the final executable.
     * "temp" stores the offset of each file's section as it would appear
     * in the final executable.
     */
    for(int i = 0; i < exec->symbols.num_symbols; i++) {
        if (symbols->shndx != shndx[symbols->fndx]) {
            symbols = symbols->next;
            continue;
        }

        symbols->value += exec->text_vaddr + offset[symbols->fndx];
        symbols = symbols->next;
    }

    /*
     * Do the same thing for other sections (e.g. ".data"). Ensure the section starts
     * at a page boundary.
     */
    exec->data_vaddr = exec->text_vaddr + ((exec->text_size + page_size - 1) / page_size) * page_size;
    elf = elves->head;

    for(int i = 0; i < elves->num_files; i++) {
        shndx[i] = elf->data.index;
        offset[i] = exec->data_size;
        exec->data_size += elf->data.size;
        elf = elf->next;
    }

    symbols = exec->symbols.head;

    for(int i = 0; i < exec->symbols.num_symbols; i++) {
        if (symbols->shndx != shndx[symbols->fndx]) {
            symbols = symbols->next;
            continue;
        }

        symbols->value += exec->data_vaddr + offset[symbols->fndx];
        symbols = symbols->next;
    }

#ifdef DEBUG
    printf("text -> 0x%lx of len 0x%x\n", exec->text_vaddr, exec->text_size);
    printf("data -> 0x%lx of len 0x%x\n", exec->data_vaddr, exec->data_size);

    symbols = exec->symbols.head;
    while(symbols) {
        printf("---------------\n");
        printf("%s\n", symbols->name);
        printf("0x%x\n", symbols->shndx);
        printf("0x%lx\n", symbols->value);
        symbols = symbols->next;
    }
#endif

    free(shndx);
    free(offset);
}

void relocate_symbols(Elf_list *elves, Elf_exec_t *exec) {
    Elf_info_t *elf = elves->head;
    uint64_t section_offset = 0, reloc_offset = 0;
    Elf_reloctab_t *reloc_ent = NULL;
    Reloc_t *reloc = NULL, *rptr = NULL;
    int num_relocs = 0;

    for(int fndx = 0; fndx < elves->num_files; fndx++) {
        for(int i = 0; i < elf->num_rel_text; i++) {
            reloc_ent = &elf->rela_text[i];

            reloc = (Reloc_t *)malloc(sizeof(Reloc_t));
            memset((void *)reloc, 0, sizeof(Reloc_t));
            reloc->offset = section_offset + reloc_ent->offset;

            if (!exec->relocations.head) {
                exec->relocations.head = reloc;
            } else {
                rptr->next = reloc;
            }

            rptr = reloc;
            num_relocs++;
        }

        section_offset += elf->text.size;
        elf = elf->next;
    }

    elf = elves->head;

    for(int fndx = 0; fndx < elves->num_files; fndx++) {
        section_offset = elf->data.offset;

        for(int i = 0; i < elf->num_rel_data; i++) {
            reloc = (Reloc_t *)malloc(sizeof(Reloc_t));
            memset((void *)reloc, 0, sizeof(Reloc_t));
            reloc->offset = section_offset + reloc_ent->offset;

            if (!exec->relocations.head) {
                exec->relocations.head = reloc;
            } else {
                rptr->next = reloc;
            }

            rptr = reloc;
            num_relocs++;
        }

        section_offset += elf->data.size;
        elf = elf->next;
    }

    exec->relocations.num_relocs = num_relocs;
    rptr = exec->relocations.head;
    printf("Num of relocations: %d\n", exec->relocations.num_relocs);

    for(int i = 0; i < exec->relocations.num_relocs; i++) {
        printf("0x%lx <-> 0x%lx\n", rptr->offset, rptr->value);
        rptr = rptr->next;
    }
}
