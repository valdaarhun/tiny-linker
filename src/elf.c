#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "utils.h"

#ifdef DEBUG
static void print_elf_header(Elf_hdr_t *elf_header) {
    for(int i = 0; i < E_IDENT_LEN; i++) {
        printf("%hhx ", elf_header->e_ident[i]);
    }
    printf("\n");
    printf("%hx\n", elf_header->e_type);
    printf("%hx\n", elf_header->e_machine);
    printf("%x\n", elf_header->e_version);
    printf("%lx\n", elf_header->e_entry);
    printf("%lx\n", elf_header->e_phoff);
    printf("%lx\n", elf_header->e_shoff);
    printf("%x\n", elf_header->e_flags);
    printf("%hx\n", elf_header->e_ehsize);
    printf("%hx\n", elf_header->e_phentsize);
    printf("%hx\n", elf_header->e_phnum);
    printf("%hx\n", elf_header->e_shentsize);
    printf("%hx\n", elf_header->e_shnum);
    printf("%hx\n", elf_header->e_shstrndx);
}
#endif

static void read_elf_header(FILE *file, Elf_hdr_t *elf_header) {
    if (!file) return;

    uint8_t format = 0, offset = 0;
    uint8_t buffer[MAX_ELF_HDR_LEN];

    memset((void *)buffer, 0, MAX_ELF_HDR_LEN);

    fread(elf_header->e_ident, sizeof(uint8_t), 0x10, file);
    fseek(file, 0, SEEK_SET);
    offset = 0x10;

    format = elf_header->e_ident[4];
    if (format == 1) {
        fread(buffer, sizeof(uint8_t), 0x34, file);
    } else {
        fread(buffer, sizeof(uint8_t), MAX_ELF_HDR_LEN, file);
    }

    read_u16((void *)&elf_header->e_type, &buffer[offset], &offset);
    read_u16((void *)&elf_header->e_machine, &buffer[offset], &offset);
    read_u32((void *)&elf_header->e_version, &buffer[offset], &offset);

    if (format == 1) {
        read_u32((void *)&elf_header->e_entry, &buffer[offset], &offset);
        read_u32((void *)&elf_header->e_phoff, &buffer[offset], &offset);
        read_u32((void *)&elf_header->e_shoff, &buffer[offset], &offset);
    } else if (format == 2) {
        read_u64((void *)&elf_header->e_entry, &buffer[offset], &offset);
        read_u64((void *)&elf_header->e_phoff, &buffer[offset], &offset);
        read_u64((void *)&elf_header->e_shoff, &buffer[offset], &offset);
    } else {
        fprintf(stderr, "Unrecognized format: 0x%x\n", format);
        exit(1);
    }

    read_u32((void *)&elf_header->e_flags, &buffer[offset], &offset);
    read_u16((void *)&elf_header->e_ehsize, &buffer[offset], &offset);
    read_u16((void *)&elf_header->e_phentsize, &buffer[offset], &offset);
    read_u16((void *)&elf_header->e_phnum, &buffer[offset], &offset);
    read_u16((void *)&elf_header->e_shentsize, &buffer[offset], &offset);
    read_u16((void *)&elf_header->e_shnum, &buffer[offset], &offset);
    read_u16((void *)&elf_header->e_shstrndx, &buffer[offset], &offset);

#ifdef DEBUG
    print_elf_header(elf_header);
#endif
}


void read_elf(FILE *file) {
    Elf_hdr_t *elf_header = (Elf_hdr_t *)malloc(sizeof(Elf_hdr_t));
    read_elf_header(file, elf_header);
    // Elf_shdr_t *shtab = read_sections(file, elf_header);
}
