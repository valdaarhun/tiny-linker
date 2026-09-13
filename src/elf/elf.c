#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "elf/elf.h"
#include "elf.h"
#include "utils/utils.h"

/* Printer */

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
        printf("index: %d\n", i);
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

void print_symtab(Elf_symbol_t *symbols, int symnum) {
    printf("=======Symbols=======\n");
    /*
     * The symbol at index 0 of the symbol table is reserved and its
     * shndx is always set to SHN_UNDEF.
     */
    for(int i = 1; i < symnum; i++) {
        printf("index: %d\n", i);
        printf("%s\n", symbols[i].st_name);
        printf("%hhx\n", SYMBOL_BINGDING(symbols[i].st_info));
        printf("%hhx\n", SYMBOL_TYPE(symbols[i].st_info));
        printf("%hhx\n", symbols[i].st_other);
        printf("%hx\n", symbols[i].st_shndx);
        printf("%lx\n", symbols[i].st_value);
        printf("%lx\n", symbols[i].st_size);
        printf("=====================\n");
    }
}

void print_reloctab(Elf_reloctab_t *relocs, int num_relocs, char *symstr) {
    printf("=======Relocations=======\n");
    /*
     * The symbol at index 0 of the symbol table is reserved and its
     * shndx is always set to SHN_UNDEF.
     */
    for(int i = 0; i < num_relocs; i++) {
        printf("index: %d\n", i);
        printf("0x%lx\n", relocs[i].offset);
        printf("0x%x\n", relocs[i].reloc_type);
        printf("%s\n", relocs[i].st_name);
        printf("%ld\n", relocs[i].addend);
        printf("=====================\n");
    }
}
#endif

/* Reader */

static void read_elf_header(FILE *file, Elf_hdr_t *elf_header) {
    if (!file) return;

    uint8_t format = 0, offset = 0;
    uint8_t buffer[MAX_ELF_HDR_LEN];

    memset((void *)buffer, 0, MAX_ELF_HDR_LEN);

    fread(elf_header->e_ident, sizeof(uint8_t), 0x10, file);
    fseek(file, 0, SEEK_SET);
    offset = E_IDENT_LEN;

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

static Elf_symbol_t *read_symtab(FILE *file, Elf_shdr_t *symtab, Elf_shdr_t* shtab, char *symstr, char *shstr, int format) {
    uint32_t st_name;
    uint8_t buffer[symtab->sh_entsize], offset = 0;
    fseek(file, symtab->sh_offset, SEEK_SET);

    int num_symbols = symtab->sh_size / symtab->sh_entsize;
    Elf_symbol_t *symbols = (Elf_symbol_t *)calloc(num_symbols, sizeof(Elf_symbol_t));
    memset((void *)symbols, 0, num_symbols * sizeof(Elf_symbol_t));

    for(int i = 0; i < num_symbols; i++) {
        offset = 0;
        memset((void *)buffer, 0, symtab->sh_entsize);
        fread(buffer, sizeof(uint8_t), symtab->sh_entsize, file);

        if (format == 1) {
            read_u32((void *)&st_name, &buffer[offset], &offset);
            read_u32((void *)&symbols[i].st_value, &buffer[offset], &offset);
            read_u32((void *)&symbols[i].st_size, &buffer[offset], &offset);
            read_u8((void *)&symbols[i].st_info, &buffer[offset], &offset);
            read_u8((void *)&symbols[i].st_other, &buffer[offset], &offset);
            read_u16((void *)&symbols[i].st_shndx, &buffer[offset], &offset);
        } else {
            read_u32((void *)&st_name, &buffer[offset], &offset);
            read_u8((void *)&symbols[i].st_info, &buffer[offset], &offset);
            read_u8((void *)&symbols[i].st_other, &buffer[offset], &offset);
            read_u16((void *)&symbols[i].st_shndx, &buffer[offset], &offset);
            read_u64((void *)&symbols[i].st_value, &buffer[offset], &offset);
            read_u64((void *)&symbols[i].st_size, &buffer[offset], &offset);
        }

        if (SYMBOL_TYPE(symbols[i].st_info) == STT_SECTION)
            symbols[i].st_name = &shstr[shtab[symbols[i].st_shndx].sh_name];
        else
            symbols[i].st_name = &symstr[st_name];
    }

    return symbols;
}

static Elf_reloctab_t *read_reloctab(FILE *file, Elf_shdr_t *reloctab, Elf_symbol_t *symbols) {
    uint8_t buffer[reloctab->sh_entsize], offset = 0;
    uint64_t info;
    fseek(file, reloctab->sh_offset, SEEK_SET);

    int num_relocs = reloctab->sh_size / reloctab->sh_entsize;
    Elf_reloctab_t *relocs = (Elf_reloctab_t *)calloc(num_relocs, sizeof(Elf_reloctab_t));
    memset((void *)relocs, 0, num_relocs * sizeof(Elf_reloctab_t));

    for(int i = 0; i < num_relocs; i++) {
        offset = 0;
        memset((void *)buffer, 0, reloctab->sh_entsize);
        fread(buffer, sizeof(uint8_t), reloctab->sh_entsize, file);

        read_u64((void *)&relocs[i].offset, &buffer[offset], &offset);
        read_u64((void *)&info, &buffer[offset], &offset);
        read_u64((void *)&relocs[i].addend, &buffer[offset], &offset);

        relocs[i].reloc_type = RELOC_TYPE(info);
        relocs[i].st_name = symbols[RELOC_SYMTAB_NDX(info)].st_name;
    }

    return relocs;
}

static char *read_strtab(FILE *file, long addr, uint64_t size) {
    char *shstr = (char *)calloc(size, sizeof(char));
    memset((void *)shstr, 0, size);

    fseek(file, addr, SEEK_SET);
    fread(shstr, sizeof(char), size, file);
    return shstr;
}

Elf_info_t *read_elf(FILE *file) {
    Elf_info_t *elf;
    Elf_hdr_t *elf_header;
    Elf_shdr_t *shtab, *shstrtab, *strtab, *symtab, *reloctab;
    Elf_symbol_t *symbols;
    Elf_reloctab_t *rela_text = NULL, *rela_data = NULL;
    char *shstr, *symstr;
    int strtab_ndx = -1, symtab_ndx = -1, rela_text_ndx = -1, rela_data_ndx = -1;
    
    elf_header = (Elf_hdr_t *)malloc(sizeof(Elf_hdr_t));
    read_elf_header(file, elf_header);

    shtab = read_sections(file, elf_header);
    if (!shtab) {
        fprintf(stderr, "No sections found\n");
        free(elf_header);
        free(shtab);
        return NULL;
    }

    /*
     * shstrtab is the section header table entry that describes the table used
     * to store section header names. The tables is effectively a list of names
     * delimited by '\0'. The value stored in the name field of each section
     * header entry is an offset in the shstr table.
     */
    shstrtab = &shtab[elf_header->e_shstrndx];
    shstr = read_strtab(file, shstrtab->sh_offset, shstrtab->sh_size);

#ifdef DEBUG
    print_shtab(shtab, elf_header->e_shnum, shstr);
#endif

    elf = (Elf_info_t *)malloc(sizeof(Elf_info_t));
    memset((void *)elf, 0, sizeof(Elf_info_t));
    elf->hdr = elf_header;
    elf->shstr = shstr;

    for(int i = 0; i < elf_header->e_shnum; i++) {
        if (!strcmp(".strtab", &shstr[shtab[i].sh_name])) {
            strtab_ndx = i;
        } else if (!strcmp(".symtab", &shstr[shtab[i].sh_name])) {
            symtab_ndx = i;
        } else if (!strcmp(".text", &shstr[shtab[i].sh_name])) {
            elf->text.name = shtab[i].sh_name;
            elf->text.offset = shtab[i].sh_offset;
            elf->text.size = shtab[i].sh_size;
            elf->text.index = i;
        } else if (!strcmp(".rela.text", &shstr[shtab[i].sh_name])) {
            rela_text_ndx = i;
        } else if (!strcmp(".data", &shstr[shtab[i].sh_name])) {
            elf->data.name = shtab[i].sh_name;
            elf->data.offset = shtab[i].sh_offset;
            elf->data.size = shtab[i].sh_size;
            elf->data.index = i;
        } else if (!strcmp(".rela.data", &shstr[shtab[i].sh_name])) {
            rela_data_ndx = i;
        } // else if (!strcmp(".bss", &shstr[shtab[i].sh_name])) {
        //     elf->sections[2].sh_name = shtab[i].sh_name;
        //     elf->sections[2].sh_offset = shtab[i].sh_offset;
        //     elf->sections[2].sh_size = shtab[i].sh_size;
        // }
    }

    strtab = &shtab[strtab_ndx];
    symstr = read_strtab(file, strtab->sh_offset, strtab->sh_size);
    elf->symstr = symstr;

    symtab = &shtab[symtab_ndx];
    symbols = read_symtab(file, symtab, shtab, symstr, shstr, elf_header->e_ident[4]);
#ifdef DEBUG
    print_symtab(symbols, symtab->sh_size / symtab->sh_entsize);
#endif

    elf->num_symbols = symtab->sh_size / symtab->sh_entsize;
    // for(int i = 1; i < elf->num_symbols; i++) {
    //     if (symbols[i].st_shndx == 0 || (symbols[i].st_shndx >> 8) == 0xff)
    //         continue;
    //     unsigned char type = symbols[i].st_info & 0xf;
    //     if (type == STT_SECTION || type == STT_FILE)
    //         continue;
    // }
    elf->symbols = symbols;

    if (rela_text_ndx != -1) {
        reloctab = &shtab[rela_text_ndx];
        rela_text = read_reloctab(file, reloctab, symbols);
#ifdef DEBUG
        print_reloctab(rela_text, reloctab->sh_size / reloctab->sh_entsize, symstr);
#endif

        elf->num_rel_text = reloctab->sh_size / reloctab->sh_entsize;
        elf->rela_text = rela_text;
    }

    if (rela_data_ndx != -1) {
        reloctab = &shtab[rela_data_ndx];
        rela_data = read_reloctab(file, reloctab, symbols);
#ifdef DEBUG
        print_reloctab(rela_data, reloctab->sh_size / reloctab->sh_entsize, symstr);
#endif

        elf->num_rel_data = reloctab->sh_size / reloctab->sh_entsize;
        elf->rela_data = rela_data;
    }

    free(shtab);

    return elf;
}

/* Writer */

static void write_elf_header(FILE *file, Elf_hdr_t* hdr) {
    uint8_t buffer[MAX_ELF_HDR_LEN] = { 0 };
    uint8_t offset = E_IDENT_LEN, format = hdr->e_ident[4];
    uint64_t temp;

    memcpy((void *)buffer, (void *)hdr->e_ident, sizeof(uint8_t) * E_IDENT_LEN);

    temp = ET_EXEC;
    write_u16(temp, buffer, &offset); // Set file type in header
    write_u16(hdr->e_machine, buffer, &offset);
    write_u32(hdr->e_version, buffer, &offset);

    if (format == 1) {
        temp = 0x0;
        write_u32(temp, buffer, &offset); // Set start address
        temp = 0x34;
        write_u32(temp, buffer, &offset); // Set program header offset
        temp = 0x0;
        write_u32(temp, buffer, &offset); // Set section header offset to 0
    } else {
        temp = 0x0;
        write_u64(temp, buffer, &offset); // Set start address
        temp = 0x40;
        write_u64(temp, buffer, &offset); // Set program header offset
        temp = 0x0;
        write_u64(temp, buffer, &offset); // Set section header offset to 0
    }

    write_u32(hdr->e_flags, buffer, &offset);
    write_u16(hdr->e_ehsize, buffer, &offset); // Size of header
    write_u16(hdr->e_phentsize, buffer, &offset); // Program header entry size
    write_u16(hdr->e_phnum, buffer, &offset); // Num of entries in program header
    temp = 0;
    write_u16(temp, buffer, &offset); // Section header entry size (0 for our exec)
    write_u16(temp, buffer, &offset); // Num of entries in section header (0 for our exec)
    write_u16(temp, buffer, &offset);

    fwrite((void *)buffer, sizeof(uint8_t), sizeof(buffer), file);
}

static void write_section(FILE *fout, FILE *fin, Section_info_t *section, uint64_t *offset) {
    *offset += section->size;

    char *res = mmap(NULL, sizeof(uint8_t) * (section->offset + section->size),
                     PROT_READ, MAP_PRIVATE, fileno(fin), 0);

    if (res == MAP_FAILED) {
        perror("Failed to map file chunk");
        exit(-1);
    }

    fwrite(res + section->offset, sizeof(uint8_t), section->size, fout);
    if (munmap(res, section->offset + section->size) < 0) {
        perror("Failed to unmap memory");
        exit(-1);
    }
}

static void write_sections(FILE *file, Elf_exec_t *exec, Elf_info_t *elf, int num_files, uint64_t *section_end_offset) {
    FILE *fds[num_files];
    Elf_info_t *ptr = elf;
    uint64_t offset = 0x1000;

    // Merge and write .text section
    memset((void *)fds, 0, sizeof(int) * num_files);
    fseek(file, offset, SEEK_SET);
    exec->aux_data.text_offset = offset;

    for(int i = 0; i < num_files; i++) {
        fds[i] = fopen(ptr->file_name, "r");
        write_section(file, fds[i], &ptr->text, &offset);

        ptr = ptr->next;
    }

    offset = (offset + page_size - 1) / page_size * page_size;
    fseek(file, offset, SEEK_SET);
    exec->aux_data.data_offset = offset;

    ptr = elf;

    // Merge and write .data section
    for(int i = 0; i < num_files; i++) {
        write_section(file, fds[i], &ptr->data, &offset);
        fclose(fds[i]);

        ptr = ptr->next;
    }

    *section_end_offset = offset;
}

static void write_symtab(FILE *file, Elf_exec_t *exec, uint64_t symtab_offset) {
    int format = exec->hints->e_ident[4], temp, num_symbols = exec->symbols.num_symbols;
    uint8_t buffer_ptr = 0;
    uint8_t buffer[SYMTAB_ENT_SZ_64] = { 0 };
    uint64_t offset = 0;
    Symbol_t *ptr = exec->symbols.head;

    if (num_symbols == 0) {
        return;
    }

    // Align symtab on 8-byte boundary
    if (format == 1) {
        symtab_offset = (symtab_offset + sizeof(uint32_t) - 1) / sizeof(uint32_t) * sizeof(uint32_t);
    } else {
        symtab_offset = (symtab_offset + sizeof(uint64_t) - 1) / sizeof(uint64_t) * sizeof(uint64_t);
    }

    exec->aux_data.symtab_offset = symtab_offset;
    fseek(file, symtab_offset, SEEK_SET);

    // offset will now point to end of symtab
    if (format == 1) {
        offset = symtab_offset + ((num_symbols + 1) * SYMTAB_ENT_SZ_32);
        fwrite(buffer, sizeof(uint8_t), SYMTAB_ENT_SZ_32, file);
    } else {
        offset = symtab_offset + ((num_symbols + 1) * SYMTAB_ENT_SZ_64);
        fwrite(buffer, sizeof(uint8_t), SYMTAB_ENT_SZ_64, file);
    }

    for(int i = 0; i < num_symbols; i++) {
        buffer_ptr = 0;
        if (format == 1) {
            temp = 0;
            write_u32(temp, buffer, &buffer_ptr);
            write_u32(ptr->value, buffer, &buffer_ptr);
            write_u32(ptr->size, buffer, &buffer_ptr);
            write_u8(ptr->info, buffer, &buffer_ptr);
            write_u8(ptr->other, buffer, &buffer_ptr);
            write_u16(ptr->shndx, buffer, &buffer_ptr);
            fwrite(buffer, sizeof(uint8_t), SYMTAB_ENT_SZ_32, file);
        } else {
            temp = 0;
            write_u32(temp, buffer, &buffer_ptr);
            write_u8(ptr->info, buffer, &buffer_ptr);
            write_u8(ptr->other, buffer, &buffer_ptr);
            write_u16(ptr->shndx, buffer, &buffer_ptr);
            write_u64(ptr->value, buffer, &buffer_ptr);
            write_u64(ptr->size, buffer, &buffer_ptr);
            fwrite(buffer, sizeof(uint8_t), SYMTAB_ENT_SZ_64, file);
        }

        ptr = ptr->next;
    }

    exec->aux_data.symstr_offset = offset;
    ptr = exec->symbols.head;

    fputc('\0', file);
    offset++;

    for(int i = 0; i < num_symbols; i++) {
        if (SYMBOL_TYPE(ptr->info) != STT_SECTION) {
            fputs(ptr->name, file);
            fputc('\0', file);
            offset += strlen(ptr->name) + 1;
        }
        ptr = ptr->next;
    }

    exec->aux_data.shstr_offset = offset;
    printf("%lx\n", exec->aux_data.symstr_offset);
    printf("%lx\n", exec->aux_data.shstr_offset);
}

static void write_shstr(FILE *file, Elf_exec_t *exec) {
    int format = exec->hints->e_ident[4];
    uint64_t offset = exec->aux_data.shstr_offset;
    fseek(file, offset, SEEK_SET);
    fputc('\0', file);
    offset++;

    if (exec->text_size > 0) {
        fputs(".text", file);
        fputc('\0', file);
        offset += strlen(".text") + 1;
    }

    if (exec->data_size > 0) {
        fputs(".data", file);
        fputc('\0', file);
        offset += strlen(".data") + 1;
    }

    if (exec->symbols.num_symbols > 0) {
        fputs("symtab", file);
        fputc('\0', file);

        fputs("symstr", file);
        fputc('\0', file);

        offset += strlen("symstr") + strlen("symtab") + 1;
    }

    fputs("shstr", file);
    fputc('\0', file);
    offset += strlen("shstr") + 1;

    // Align shdr on 8-byte boundary
    if (format == 1) {
        offset = (offset + sizeof(uint32_t) - 1) / sizeof(uint32_t) * sizeof(uint32_t);
    } else {
        offset = (offset + sizeof(uint64_t) - 1) / sizeof(uint64_t) * sizeof(uint64_t);
    }

    exec->aux_data.shtab_offset = offset;
}

// static void write_shdr(FILE *file, Elf_exec_t *exec) {
//     int shent_sz = exec->hints->e_shentsize;
//     uint8_t buffer[shent_sz];

//     memset((void *)buffer, 0, sizeof(uint8_t) * shent_sz);
// }

void write_elf(Elf_exec_t *exec, Elf_list *elves) {
    Elf_info_t *ptr = elves->head;
    FILE *file = fopen("a.out", "w");
    uint64_t symtab_offset = 0;

    write_elf_header(file, exec->hints);
    write_sections(file, exec, elves->head, elves->num_files, &symtab_offset);
    write_symtab(file, exec, symtab_offset);
    write_shstr(file, exec);
    // write_shdr(file, exec);

    fclose(file);
}
