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

static int insert_symbol(uint16_t fndx, Symbol_t **sl_head, Symbol_t **sl_tail, Elf_symbol_t *elf_symbol, uint32_t num) {
    int fl = 0;
    Symbol_t *ptr = *sl_head;
    char *sym_name = elf_symbol->st_name;

    if (RELOC_TYPE(elf_symbol->st_info) != STT_SECTION) {
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

static Symbol_t *get_symbol_vaddr(char *name, uint16_t fndx, Symbol_t *sptr) {
    while(sptr) {
        if (!strcmp(name, sptr->name) && SYMBOL_TYPE(sptr->info) != STT_SECTION) {
            return sptr;
        }

        if (!strcmp(name, sptr->name)
            && SYMBOL_TYPE(sptr->info) == STT_SECTION
            && sptr->fndx == fndx
        ) {
            return sptr;
        }

        sptr = sptr->next;
    }

    return NULL;
}

static void assign_vaddr_internal(Elf_list *elves, Elf_exec_t *exec, uint64_t addr, Segment_type_t segment) {
    Elf_info_t *elf = elves->head;
    Symbol_t *symbols = exec->symbols.head;
    uint32_t *shndx = (uint32_t *)calloc(elves->num_files, sizeof(uint32_t));
    uint64_t *offset = (uint64_t *)calloc(elves->num_files, sizeof(uint64_t));

    memset((void *)shndx, 0, elves->num_files * sizeof(uint32_t));
    memset((void *)offset, 0, elves->num_files * sizeof(uint64_t));

    exec->segment_vaddr[segment] = addr;

    for(int i = 0; i < elves->num_files; i++) {
        shndx[i] = elf->sections[segment].index;
        offset[i] = exec->segment_size[segment];
        exec->segment_size[segment] += elf->sections[segment].size;
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

        symbols->value += exec->segment_vaddr[segment] + offset[symbols->fndx];
        symbols->shndx = segment; // segment index in final executable
        symbols = symbols->next;
    }

    free(shndx);
    free(offset);
}

static int relocate_symbols_internal(Elf_list *elves, Elf_exec_t *exec, Segment_type_t segment) {
    Elf_info_t *elf = elves->head;
    Elf_reloctab_t *reloc_ent = NULL;
    Elf_reloctab_t *relocations;
    Symbol_t *symbol;
    Reloc_t *reloc, *rptr = exec->relocations.tail;
    uint64_t section_offset = 0;
    int num_relocs = 0;

    for(int fndx = 0; fndx < elves->num_files; fndx++) {
        for(int i = 0; i < elf->num_relocations[segment]; i++) {
            relocations = elf->relocations[segment];
            reloc_ent = &relocations[i];
            symbol = get_symbol_vaddr(reloc_ent->st_name, fndx, exec->symbols.head);

            if (!symbol) {
                fprintf(stderr, "Couldn't find relocation info for symbol '%s'\n", reloc_ent->st_name);
                return -1;
            }

            reloc = (Reloc_t *)malloc(sizeof(Reloc_t));
            memset((void *)reloc, 0, sizeof(Reloc_t));

            reloc->name = reloc_ent->st_name;
            reloc->section_type = segment;
            reloc->offset = section_offset + reloc_ent->offset;

            switch(reloc_ent->reloc_type) {
            case R_X86_64_PLT32:
            case R_X86_64_PC32:
                reloc->value = (uint32_t)(symbol->value + reloc_ent->addend - (exec->segment_vaddr[segment] + reloc->offset));
                break;
            case R_X86_64_32:
            case R_X86_64_32S:
                reloc->value = (uint32_t)(symbol->value + reloc_ent->addend);
                break;
            default:
                fprintf(stderr,
                        "Relocation type %d is not supported at the moment\n",
                        reloc_ent->reloc_type);
                return -1;
            }

            if (!exec->relocations.head)
                exec->relocations.head = reloc;
            else
                rptr->next = reloc;

            rptr = reloc;
            num_relocs++;
        }

        section_offset += elf->sections[segment].size;
        elf = elf->next;
    }

    exec->relocations.tail = rptr;

    return num_relocs;
}

int create_symtab(Elf_list *elves, Elf_exec_t *exec) {
    Elf_info_t *elf = elves->head;
    Symbol_t *sl_tail = NULL, *sptr;
    Symbol_t *local_head = NULL, *local_tail = NULL, *non_local_head = NULL, *non_local_tail = NULL;
    uint32_t num_symbols = 0;
    Elf_symbol_t *elf_symbols;

    for(int fndx = 0; fndx < elves->num_files; fndx++) {
        int cnt = 0;
        elf_symbols = elf->symbols;

        for(int i = 1; i < elf->num_symbols; i++) {
            if ((elf_symbols[i].st_shndx >> 8) == SHN_SPECIAL)
                continue;

            if (SYMBOL_TYPE(elf_symbols[i].st_info) == STT_FILE)
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

    while(sptr) {
        if (sptr->shndx == SHN_UNDEF) {
            fprintf(stderr, "Couldn't find definition for symbol '%s'\n", sptr->name);
            return -1;
        }

        sptr = sptr->next;
    }

    sptr = exec->symbols.head;
    while(sptr) {
        if (SYMBOL_BINGDING(sptr->info) == STB_LOCAL) {
            if (!local_head)
                local_head = sptr;
            else
                local_tail->next = sptr;

            local_tail = sptr;
        } else {
            if (!non_local_head)
                non_local_head = sptr;
            else
                non_local_tail->next = sptr;

            non_local_tail = sptr;
        }

        sptr = sptr->next;
    }

    if (local_head) {
        exec->symbols.head = local_head;
        local_tail->next = non_local_head;
    } else {
        exec->symbols.head = non_local_head;
    }

    if (non_local_tail)
        non_local_tail->next = NULL;

#ifdef DEBUG
    printf("Num of symbols: %d\n", exec->symbols.num_symbols);

    sptr = exec->symbols.head;
    while(sptr) {
        printf("===============\n");
        printf("Symbol name: %s\n", sptr->name);
        printf("Symbol binding: %d\n", SYMBOL_BINGDING(sptr->info));
        printf("Associated section: 0x%x\n", sptr->shndx);
        printf("Symbol value: 0x%lx\n", sptr->value);

        sptr = sptr->next;
    }

    printf("===============\n");
#endif

    return 0;
}

int assign_vaddr(Elf_list *elves, Elf_exec_t *exec) {
    Symbol_t *symbol = exec->symbols.head;
    uint64_t addr = START_ADDR;

    assign_vaddr_internal(elves, exec, addr, TEXT);

    /*
     * Once a vaddr has been assigned to a section, ensure the following section starts
     * at a page boundary.
     */
    addr = exec->segment_vaddr[TEXT] + ((exec->segment_size[TEXT] + page_size - 1) / page_size) * page_size;
    assign_vaddr_internal(elves, exec, addr, DATA);

#ifdef DEBUG
    printf("text -> 0x%lx of len 0x%x\n", exec->segment_vaddr[TEXT], exec->segment_size[TEXT]);
    printf("data -> 0x%lx of len 0x%x\n", exec->segment_vaddr[DATA], exec->segment_size[DATA]);

    while(symbol) {
        printf("---------------\n");
        printf("%s\n", symbol->name);
        printf("0x%x\n", symbol->shndx);
        printf("0x%lx\n", symbol->value);
        symbol = symbol->next;
    }

    printf("---------------\n");

    symbol = exec->symbols.head;
#endif

    for(int i = 0; i < exec->symbols.num_symbols; i++) {
        if (!strcmp(symbol->name, "_start")) {
            exec->aux_data.start_addr = symbol->value;
            return 0;
        }

        symbol = symbol->next;
    }

    fprintf(stderr, "Couldn't find '_start' symbol\n");
    return -1;
}

int relocate_symbols(Elf_list *elves, Elf_exec_t *exec) {
    Reloc_t *rptr = NULL;
    int res = 0;

    res = relocate_symbols_internal(elves, exec, TEXT);
    if (res < 0)
        return res;

    exec->relocations.num_relocs = res;

    res = relocate_symbols_internal(elves, exec, DATA);
    if (res < 0)
        return res;

    exec->relocations.num_relocs += res;

#ifdef DEBUG
    rptr = exec->relocations.head;
    printf("Num of relocations: %d\n", exec->relocations.num_relocs);

    for(int i = 0; i < exec->relocations.num_relocs; i++) {
        printf("Name: %s\n", rptr->name);
        printf("Offset: 0x%lx <-> Value: 0x%lx\n", rptr->offset, rptr->value);
        rptr = rptr->next;
    }
#endif

    return 0;
}
