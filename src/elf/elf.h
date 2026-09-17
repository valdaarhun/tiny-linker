#ifndef ELF_H
#define ELF_H

#include <stdio.h>
#include <stdint.h>

/*
 * ------------------------------
 * ELF-specific utils
 * ------------------------------
 */

extern int page_size;

#define NUM_SEGMENTS    2 // 0 -> .text, 1 -> .data

typedef enum {
    TEXT,
    DATA
} Segment_type_t;

/*
 * ------------------------------
 * Structs used while parsing ELF
 * ------------------------------
 */

#define E_IDENT_LEN 0x10
#define MAX_ELF_HDR_LEN 0x40

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

#define SYMTAB_ENT_SZ_32    16
#define SYMTAB_ENT_SZ_64    24

typedef struct {
    uint64_t offset;
    uint32_t reloc_type;
    char* st_name;
    int64_t addend;
} Elf_reloctab_t;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t size;
    uint64_t memsz;
    uint64_t align;
} Elf_phdr_t;

#define PHDR_ENT_SZ_32    32
#define PHDR_ENT_SZ_64    56

#define PT_NULL     0x0
#define PT_LOAD     0x1
#define PT_PHDR     0x6

#define PF_X    0x1
#define PF_W    0x2
#define PF_R    0x4

#define ELF_HDR_SHOFF_OFFSET_32     0x20
#define ELF_HDR_SHOFF_OFFSET_64     0x28
#define ELF_HDR_PHENTSZ_OFFSET_32   0x2a
#define ELF_HDR_PHENTSZ_OFFSET_64   0x36
#define ELF_HDR_SHNUM_OFFSET_32     0x30
#define ELF_HDR_SHNUM_OFFSET_64     0x3c
#define ELF_HDR_SHSTRNDX_OFFSET_32  0x32
#define ELF_HDR_SHSTRNDX_OFFSET_64  0x3e

#define ELF_PHOFF_32     0x34
#define ELF_PHOFF_64     0x40

#define ET_REL      0x1
#define ET_EXEC     0x2

#define SHT_PROGBITS    0x1
#define SHT_SYMTAB      0x2
#define SHT_STRTAB      0x3

#define SHF_WRITE       0x1
#define SHF_ALLOC       0x2
#define SHF_EXECINSTR   0x4

#define SYMBOL_BINGDING(ent)    ((ent) >> 4)
#define SYMBOL_TYPE(ent)        ((ent) & 0xf)

#define SHN_UNDEF       0       // Undefined section, implies that the symbol is defined in another file
#define SHN_SPECIAL     0xff    // Sections for these special indexes don't exist

#define STT_SECTION     3   // Symbol signifying a section
#define STT_FILE        4   // Symbol signifying a file

#define STB_LOCAL   0   // Symbol local to the object file that defines it

#define RELOC_SYMTAB_NDX(ent)   ((ent) >> 32)
#define RELOC_TYPE(ent)         ((ent) & 0xffffffff)

#define R_X86_64_PC32   2
#define R_X86_64_PLT32  4
#define R_X86_64_32     10
#define R_X86_64_32S    11

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
    char *file_name;
    char *shstr;
    char *symstr;
    Elf_hdr_t *hdr;
    Elf_symbol_t *symbols;
    Elf_reloctab_t *relocations[NUM_SEGMENTS];
    struct Elf_info_t *next;

    uint32_t num_symbols;
    uint32_t num_relocations[NUM_SEGMENTS];
    Section_info_t sections[NUM_SEGMENTS];
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
    /*
     * Store index of file that holds symbol's definition, especially useful for
     * symbols representing sections
     */
    uint16_t fndx;
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

struct Reloc_t {
    uint64_t offset;
    uint64_t value;
    char *name;
    struct Reloc_t *next;
    uint16_t section_type;
};

typedef struct Reloc_t Reloc_t;

typedef struct {
    uint32_t num_relocs;
    Reloc_t *head;
    Reloc_t *tail;
} Reloctab_t;

typedef struct {
    uint64_t text_offset;
    uint64_t data_offset;
    uint64_t symtab_offset;
    uint64_t symstr_offset;
    uint64_t shstr_offset;
    uint64_t shtab_offset;
    uint64_t start_addr;
    char **shstr;
    int symstr_len;
    int shstr_len;
    int recorded_symbols;
    int recorded_local_symbols;
} Elf_exec_aux_t;

typedef struct {
    Elf_hdr_t *hints;
    Symtab_t symbols;
    Reloctab_t relocations;
    uint64_t segment_vaddr[NUM_SEGMENTS];
    uint32_t segment_size[NUM_SEGMENTS];
    Elf_exec_aux_t aux_data;
} Elf_exec_t;

/*
 * Functions
 */
Elf_info_t *read_elf(FILE *);
int write_elf(Elf_exec_t *, Elf_list *);

#endif
