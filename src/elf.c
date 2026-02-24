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

void print_shtab(Elf_shdr_t *shtab, uint16_t shnum, char *shstr) {
    printf("=======Section headers=======\n");
    for(int i = 0; i < shnum; i++) {
        if (shtab[i].sh_type == 0) {
            continue;
        }
        printf("index: %hd\n", i);
        printf("%s\n", &shstr[shtab[i].sh_name]);
        printf("%x\n", shtab[i].sh_type);
        printf("%lx\n", shtab[i].sh_flags);
        printf("%lx\n", shtab[i].sh_addr);
        printf("%lx\n", shtab[i].sh_offset);
        printf("%lx\n", shtab[i].sh_size);
        printf("%x\n", shtab[i].sh_link);
        printf("%x\n", shtab[i].sh_info);
        printf("%lx\n", shtab[i].sh_addralign);
        printf("%lx\n", shtab[i].sh_entsize);
        printf("=====================\n");
    }
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

static Elf_shdr_t *read_sections(FILE *file, Elf_hdr_t *elf_header) {
    // Go to start of section header table, if it exists.
    if (elf_header->e_shnum == 0) {
        fprintf(stderr, "No section header table found\n");
        return NULL;
    }

    uint16_t shsize = elf_header->e_shentsize;
    uint8_t buffer[shsize], offset = 0;
    Elf_shdr_t *shtab = (Elf_shdr_t *)calloc(shsize, sizeof(Elf_shdr_t));

    fseek(file, elf_header->e_shoff, SEEK_SET);
    for(int i = 0; i < elf_header->e_shnum; i++) {
        offset = 0;
        memset((void *)buffer, 0, shsize);
        fread(buffer, sizeof(uint8_t), shsize, file);
        read_u32((void *)&shtab[i].sh_name, &buffer[offset], &offset);
        read_u32((void *)&shtab[i].sh_type, &buffer[offset], &offset);
        if (elf_header->e_ident[4] == 1) {
            read_u32((void *)&shtab[i].sh_flags, &buffer[offset], &offset);
            read_u32((void *)&shtab[i].sh_addr, &buffer[offset], &offset);
            read_u32((void *)&shtab[i].sh_offset, &buffer[offset], &offset);
            read_u32((void *)&shtab[i].sh_size, &buffer[offset], &offset);
            read_u32((void *)&shtab[i].sh_link, &buffer[offset], &offset);
            read_u32((void *)&shtab[i].sh_info, &buffer[offset], &offset);
            read_u32((void *)&shtab[i].sh_addralign, &buffer[offset], &offset);
            read_u32((void *)&shtab[i].sh_entsize, &buffer[offset], &offset);
        } else if (elf_header->e_ident[4] == 2) {
            read_u64((void *)&shtab[i].sh_flags, &buffer[offset], &offset);
            read_u64((void *)&shtab[i].sh_addr, &buffer[offset], &offset);
            read_u64((void *)&shtab[i].sh_offset, &buffer[offset], &offset);
            read_u64((void *)&shtab[i].sh_size, &buffer[offset], &offset);
            read_u32((void *)&shtab[i].sh_link, &buffer[offset], &offset);
            read_u32((void *)&shtab[i].sh_info, &buffer[offset], &offset);
            read_u64((void *)&shtab[i].sh_addralign, &buffer[offset], &offset);
            read_u64((void *)&shtab[i].sh_entsize, &buffer[offset], &offset);
        }
    }

    return shtab;
}

static char *read_shstrtab(FILE *file, long addr, uint64_t size) {
    char *shstr = (char *)calloc(size, sizeof(char));
    memset((void *)shstr, 0, size);

    fseek(file, addr, SEEK_SET);
    fread(shstr, sizeof(char), size, file);
    return shstr;
}

void read_elf(FILE *file) {
    Elf_hdr_t *elf_header = (Elf_hdr_t *)malloc(sizeof(Elf_hdr_t));
    read_elf_header(file, elf_header);

    Elf_shdr_t *shtab = read_sections(file, elf_header);
    if (!shtab) {
        return;
    }

    Elf_shdr_t *shstrtab = &shtab[elf_header->e_shstrndx];
    char *shstr = read_shstrtab(file, shstrtab->sh_offset, shstrtab->sh_size);

#ifdef DEBUG
    print_shtab(shtab, elf_header->e_shnum, shstr);
#endif
}
