#ifndef ELF_H
#define ELF_H

#include "stdio.h"
#include "stdint.h"

#define E_IDENT_LEN 0x10
#define MAX_ELF_HDR_LEN 0x40

typedef struct {
    uint8_t e_ident[E_IDENT_LEN];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf_hdr_t;

void read_elf(FILE *);

#endif
