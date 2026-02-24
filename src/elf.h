#ifndef ELF_H
#define ELF_H

#include "stdio.h"
#include "stdint.h"

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
    uint16_t e_shstrndx;
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
    uint64_t sh_entsize;
} Elf_shdr_t;

void read_elf(FILE *);

#endif
