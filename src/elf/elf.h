#ifndef ELF_H
#define ELF_H

#include <stdio.h>
#include <stdint.h>

#define E_IDENT_LEN 0x10
#define MAX_ELF_HDR_LEN 0x40

/*
 * ------------------------------
 * Structs used while parsing ELF
 * ------------------------------
 */
typedef struct {
    uint8_t e_ident[E_IDENT_LEN];
    uint16_t e_type; // Type of ELF
    uint16_t e_machine; // Arch
    uint32_t e_version; // ELF version
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff; // Offset of section header table
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize; // Size of each entry in section header table
    uint16_t e_shnum; // Number of entries in section header table
    uint16_t e_shstrndx; // Index of the section header table entry that contains section names.
} Elf_hdr_t;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type; // Type of header
    uint64_t sh_flags; // section attributes
    uint64_t sh_addr; // virtual address of sections that are loaded
    uint64_t sh_offset; // offset of section in file
    uint64_t sh_size; // size of section in bytes
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize; // Size of entry in case the section is a table of fixed-size entries
} Elf_shdr_t;

/*
 * The struct's fields probably has a different layout for
 * 32-bit ELF files.
 */
typedef struct {
    char* st_name;
    uint8_t st_info; // Contains bind and type info
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf_symbol_t;

typedef struct {
    uint64_t offset;
    uint32_t reloc_type;
    char* st_name;
    int64_t addend;
} Elf_reloctab_t;

#define SYMBOL_BINGDING(ent)    ((ent) >> 4)
#define SYMBOL_TYPE(ent)        ((ent) & 0xf)

#define SHN_UNDEF       0       // Undefined section, implies that the symbol is defined in another file
#define SHN_SPECIAL     0xff    // Sections for these special indexes don't exist

#define STT_SECTION     3   // Symbol signifying a section
#define STT_FILE        4   // Symbol signifying a file

#define RELOC_SYMTAB_NDX(ent)   ((ent) >> 32)
#define RELOC_TYPE(ent)         ((ent) & 0xffffffff)

/*
 * ------------------------
 * Structs used for linking
 * ------------------------
 */

#define START_ADDR  0x400000U

typedef struct {
    uint64_t offset;
    uint64_t size;
    uint64_t vaddr;
    uint32_t name;
    int index;
} Section_info_t;

struct Elf_info_t {
    char *shstr;
    char *symstr;
    Elf_symbol_t *symbols;
    Elf_reloctab_t *rela_text;
    Elf_reloctab_t *rela_data;
    struct Elf_info_t *next;
    Section_info_t text;
    Section_info_t data;
    uint32_t num_symbols;
    uint32_t num_rel_text;
    uint32_t num_rel_data;
};

typedef struct Elf_info_t Elf_info_t;

typedef struct {
    int num_files;
    Elf_info_t *head;
} Elf_list;

struct Symbol_t {
    uint8_t info; // Contains bind and type info
    uint8_t other;
    uint16_t shndx;
    uint16_t fndx; // Store index of file that holds symbol's definition
    uint64_t value;
    uint64_t size;
    char *name;
    struct Symbol_t *next;
};

typedef struct Symbol_t Symbol_t;

typedef struct {
    Symbol_t *head;
    uint32_t num_symbols;
} Symtab_t;

typedef struct {
    Symtab_t symbols;
    uint64_t text_vaddr;
    uint64_t data_vaddr;
    uint32_t text_size;
    uint32_t data_size;
} Elf_exec_t;

/*
 * Functions
 */
Elf_info_t *read_elf(FILE *);

#endif
