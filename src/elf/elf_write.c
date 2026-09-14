#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "elf.h"
#include "elf/elf.h"
#include "utils/utils.h"

/* Writer */

static void write_elf_header(FILE *file, Elf_hdr_t* hdr, uint64_t start_addr) {
    uint8_t buffer[MAX_ELF_HDR_LEN] = { 0 };
    uint8_t offset = E_IDENT_LEN, format = hdr->e_ident[4];
    uint64_t temp;

    memcpy((void *)buffer, (void *)hdr->e_ident, sizeof(uint8_t) * E_IDENT_LEN);

    temp = ET_EXEC;
    write_u16(temp, buffer, &offset); // Set file type in header
    write_u16(hdr->e_machine, buffer, &offset);
    write_u32(hdr->e_version, buffer, &offset);

    if (format == 1) {
        write_u32(start_addr, buffer, &offset); // Set start address
        temp = ELF_PHOFF_32;
        write_u32(temp, buffer, &offset); // Set program header offset
        temp = 0;
        write_u32(temp, buffer, &offset); // Set section header offset to 0
    } else {
        write_u64(start_addr, buffer, &offset); // Set start address
        temp = ELF_PHOFF_64;
        write_u64(temp, buffer, &offset); // Set program header offset
        temp = 0;
        write_u64(temp, buffer, &offset); // Set section header offset to 0
    }

    temp = 0;

    write_u32(hdr->e_flags, buffer, &offset);
    write_u16(hdr->e_ehsize, buffer, &offset); // Size of header
    write_u16(temp, buffer, &offset); // Program header entry size
    write_u16(temp, buffer, &offset); // Num of entries in program header
    write_u16(hdr->e_shentsize, buffer, &offset); // Section header entry size
    write_u16(temp, buffer, &offset); // Num of entries in section header
    write_u16(temp, buffer, &offset); // Index of shstrtab in section header table

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
    int symstr_len = 1, symstr_ndx = 1, symbols = 0;
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

    offset = symtab_offset;
    exec->aux_data.symtab_offset = offset;
    fseek(file, symtab_offset, SEEK_SET);

    // offset will now point to end of symtab
    if (format == 1) {
        fwrite(buffer, sizeof(uint8_t), SYMTAB_ENT_SZ_32, file);
        offset += SYMTAB_ENT_SZ_32;
    } else {
        fwrite(buffer, sizeof(uint8_t), SYMTAB_ENT_SZ_64, file);
        offset += SYMTAB_ENT_SZ_64;
    }

    for(int i = 0; i < num_symbols; i++) {
        buffer_ptr = 0;
        if (SYMBOL_TYPE(ptr->info) == STT_SECTION) {
            ptr = ptr->next;
            continue;
        }

        symbols++;
        temp = symstr_ndx;
        symstr_ndx += strlen(ptr->name) + 1;

        if (format == 1) {
            write_u32(temp, buffer, &buffer_ptr);
            write_u32(ptr->value, buffer, &buffer_ptr);
            write_u32(ptr->size, buffer, &buffer_ptr);
            write_u8(ptr->info, buffer, &buffer_ptr);
            write_u8(ptr->other, buffer, &buffer_ptr);
            write_u16(ptr->shndx, buffer, &buffer_ptr);
            fwrite(buffer, sizeof(uint8_t), SYMTAB_ENT_SZ_32, file);
            offset += SYMTAB_ENT_SZ_32;
        } else {
            write_u32(temp, buffer, &buffer_ptr);
            write_u8(ptr->info, buffer, &buffer_ptr);
            write_u8(ptr->other, buffer, &buffer_ptr);
            write_u16(ptr->shndx, buffer, &buffer_ptr);
            write_u64(ptr->value, buffer, &buffer_ptr);
            write_u64(ptr->size, buffer, &buffer_ptr);
            fwrite(buffer, sizeof(uint8_t), SYMTAB_ENT_SZ_64, file);
            offset += SYMTAB_ENT_SZ_64;
        }

        ptr = ptr->next;
    }

    exec->aux_data.recorded_symbols = symbols;
    exec->aux_data.symstr_offset = offset;

    fputc('\0', file);
    ptr = exec->symbols.head;

    for(int i = 0; i < num_symbols; i++) {
        if (SYMBOL_TYPE(ptr->info) == STT_SECTION) {
            ptr = ptr->next;
            continue;
        }

        fputs(ptr->name, file);
        fputc('\0', file);
        symstr_len += strlen(ptr->name) + 1;

        ptr = ptr->next;
    }

    exec->aux_data.symstr_len = symstr_len;
    exec->aux_data.shstr_offset = offset + symstr_len;
}

static void write_shstr(FILE *file, Elf_exec_t *exec) {
    char **shstr;
    int format = exec->hints->e_ident[4], shstr_len = 1, ndx = 0;
    uint64_t offset = exec->aux_data.shstr_offset;

    shstr = (char **)calloc(10, sizeof(char *));
    memset((void *)shstr, 0, sizeof(char *) * 10);

    fseek(file, offset, SEEK_SET);
    fputc('\0', file);

    if (exec->text_size > 0) {
        shstr[ndx] = ".text";
        fputs(shstr[ndx], file);
        fputc('\0', file);
        shstr_len += strlen(shstr[ndx]) + 1;
        ndx++;
    }

    if (exec->data_size > 0) {
        shstr[ndx] = ".data";
        fputs(shstr[ndx], file);
        fputc('\0', file);
        shstr_len += strlen(shstr[ndx]) + 1;
        ndx++;
    }

    if (exec->symbols.num_symbols > 0) {
        shstr[ndx] = "symtab";
        fputs(shstr[ndx], file);
        fputc('\0', file);
        shstr_len += strlen(shstr[ndx]) + 1;
        ndx++;

        shstr[ndx] = "symstr";
        fputs(shstr[ndx], file);
        fputc('\0', file);
        shstr_len += strlen(shstr[ndx]) + 1;
        ndx++;
    }

    shstr[ndx] = "shstr";
    fputs(shstr[ndx], file);
    fputc('\0', file);
    shstr_len += strlen(shstr[ndx]) + 1;
    ndx++;

    offset += shstr_len;

    // Align shdr on 8-byte boundary
    if (format == 1) {
        offset = (offset + sizeof(uint32_t) - 1) / sizeof(uint32_t) * sizeof(uint32_t);
    } else {
        offset = (offset + sizeof(uint64_t) - 1) / sizeof(uint64_t) * sizeof(uint64_t);
    }

    exec->aux_data.shstr_len = shstr_len;
    exec->aux_data.shtab_offset = offset;
    exec->aux_data.shstr = shstr;
}

// static void write_missing_symtab_names(FILE *file, Elf_exec_t *exec) {
//     int num_symbols = exec->symbols.num_symbols, format = exec->hints->e_ident[4], ndx = 1, temp = 0;
//     uint64_t offset;
//     uint8_t buffer[4] = { 0 }, buffer_ptr = 0;
//     char **shstr = exec->aux_data.shstr;
//     Symbol_t *ptr = exec->symbols.head;

//     offset = exec->aux_data.symtab_offset + (format == 1 ? SYMTAB_ENT_SZ_32 : SYMTAB_ENT_SZ_64);
//     fseek(file, offset, SEEK_SET);

//     for(int i = 0; i < num_symbols; i++) {
//         if (SYMBOL_TYPE(ptr->info) == STT_SECTION) {
//             temp = 0;
//             buffer_ptr = 0;
//             for(int j = 0; j < 10; j++) {
//                 if (shstr[j] && !strcmp(shstr[j], ptr->name)) {
//                     write_u32(ndx, buffer, &buffer_ptr);
//                     fwrite(buffer, sizeof(uint8_t), 4, file);
//                     break;
//                 }
//                 ndx += strlen(shstr[j]) + 1;
//             }
//         }

//         fseek(file, format == 1 ? SYMTAB_ENT_SZ_32 - 4 : SYMTAB_ENT_SZ_64 - 4, SEEK_CUR);
//         ptr = ptr->next;
//     }
// }

static void fill_shent_32(Elf_shdr_t* shent, uint8_t *buffer) {
    uint8_t buffer_ptr = 0;

    write_u32(shent->sh_name, buffer, &buffer_ptr);
    write_u32(shent->sh_type, buffer, &buffer_ptr);
    write_u32(shent->sh_flags, buffer, &buffer_ptr);
    write_u32(shent->sh_addr, buffer, &buffer_ptr);
    write_u32(shent->sh_offset, buffer, &buffer_ptr);
    write_u32(shent->sh_size, buffer, &buffer_ptr);
    write_u32(shent->sh_link, buffer, &buffer_ptr);
    write_u32(shent->sh_info, buffer, &buffer_ptr);
    write_u32(shent->sh_addralign, buffer, &buffer_ptr);
    write_u32(shent->sh_entsize, buffer, &buffer_ptr);
}

static void fill_shent_64(Elf_shdr_t* shent, uint8_t *buffer) {
    uint8_t buffer_ptr = 0;

    write_u32(shent->sh_name, buffer, &buffer_ptr);
    write_u32(shent->sh_type, buffer, &buffer_ptr);
    write_u64(shent->sh_flags, buffer, &buffer_ptr);
    write_u64(shent->sh_addr, buffer, &buffer_ptr);
    write_u64(shent->sh_offset, buffer, &buffer_ptr);
    write_u64(shent->sh_size, buffer, &buffer_ptr);
    write_u32(shent->sh_link, buffer, &buffer_ptr);
    write_u32(shent->sh_info, buffer, &buffer_ptr);
    write_u64(shent->sh_addralign, buffer, &buffer_ptr);
    write_u64(shent->sh_entsize, buffer, &buffer_ptr);
}

static void write_phent_32(FILE *file, Elf_phdr_t* phent, uint8_t *buffer) {
    uint8_t buffer_ptr = 0;

    write_u32(phent->type, buffer, &buffer_ptr);
    write_u32(phent->offset, buffer, &buffer_ptr);
    write_u32(phent->vaddr, buffer, &buffer_ptr);
    write_u32(phent->paddr, buffer, &buffer_ptr);
    write_u32(phent->size, buffer, &buffer_ptr);
    write_u32(phent->memsz, buffer, &buffer_ptr);
    write_u32(phent->flags, buffer, &buffer_ptr);
    write_u32(phent->align, buffer, &buffer_ptr);

    fwrite(buffer, sizeof(uint8_t), PHDR_ENT_SZ_32, file);
}

static void write_phent_64(FILE *file, Elf_phdr_t* phent, uint8_t *buffer) {
    uint8_t buffer_ptr = 0;

    write_u32(phent->type, buffer, &buffer_ptr);
    write_u32(phent->flags, buffer, &buffer_ptr);
    write_u64(phent->offset, buffer, &buffer_ptr);
    write_u64(phent->vaddr, buffer, &buffer_ptr);
    write_u64(phent->paddr, buffer, &buffer_ptr);
    write_u64(phent->size, buffer, &buffer_ptr);
    write_u64(phent->memsz, buffer, &buffer_ptr);
    write_u64(phent->align, buffer, &buffer_ptr);

    fwrite(buffer, sizeof(uint8_t), PHDR_ENT_SZ_64, file);
}

static uint32_t get_shstr_ndx(char *str, char **shstr) {
    int count = 1;
    for(int i = 0; i < 10; i++) {
        if (!strcmp(str, shstr[i]))
            return count;

        count += strlen(shstr[i]) + 1;
    }

    return 0;
}

static void write_shdr(FILE *file, Elf_exec_t *exec) {
    int shent_sz = exec->hints->e_shentsize, shstrent_ndx = 0;
    int format = exec->hints->e_ident[4], section_count = 1;
    uint8_t buffer[shent_sz], buffer_ptr = 0;
    char **shstr = exec->aux_data.shstr;

    memset((void *)buffer, 0, sizeof(uint8_t) * shent_sz);

    fseek(file, exec->aux_data.shtab_offset, SEEK_SET);
    fwrite(buffer, sizeof(uint8_t), shent_sz, file);

    if (exec->text_size > 0) {
        Elf_shdr_t shent = {
            .sh_name = get_shstr_ndx(".text", shstr),
            .sh_type = SHT_PROGBITS,
            .sh_flags = SHF_ALLOC | SHF_EXECINSTR,
            .sh_addr = exec->text_vaddr,
            .sh_offset = exec->aux_data.text_offset,
            .sh_size = exec->text_size,
            .sh_link = SHN_UNDEF,
            .sh_info = 0,
            .sh_addralign = sizeof(uint8_t),
            .sh_entsize = 0,
        };

        if (format == 1)
            fill_shent_32(&shent, buffer);
        else
            fill_shent_64(&shent, buffer);

        fwrite(buffer, sizeof(uint8_t), shent_sz, file);
        section_count++;
    }

    if (exec->data_size > 0) {
        Elf_shdr_t shent = {
            .sh_name = get_shstr_ndx(".data", shstr),
            .sh_type = SHT_PROGBITS,
            .sh_flags = SHF_ALLOC | SHF_WRITE,
            .sh_addr = exec->data_vaddr,
            .sh_offset = exec->aux_data.data_offset,
            .sh_size = exec->data_size,
            .sh_link = SHN_UNDEF,
            .sh_info = 0,
            .sh_addralign = sizeof(uint8_t),
            .sh_entsize = 0,
        };

        if (format == 1)
            fill_shent_32(&shent, buffer);
        else
            fill_shent_64(&shent, buffer);

        fwrite(buffer, sizeof(uint8_t), shent_sz, file);
        section_count++;
    }

    if (exec->symbols.num_symbols > 0) {
        Elf_shdr_t shent = {
            .sh_name = get_shstr_ndx("symtab", shstr),
            .sh_type = SHT_SYMTAB,
            .sh_flags = 0,
            .sh_addr = 0,
            .sh_offset = exec->aux_data.symtab_offset,
            .sh_size = (exec->aux_data.recorded_symbols + 1) * (format == 1 ? SYMTAB_ENT_SZ_32 : SYMTAB_ENT_SZ_64),
            .sh_link = 4,
            .sh_info = 2,
            .sh_addralign = format == 1 ? sizeof(uint32_t) : sizeof(uint64_t),
            .sh_entsize = format == 1 ? SYMTAB_ENT_SZ_32 : SYMTAB_ENT_SZ_64,
        };

        if (format == 1)
            fill_shent_32(&shent, buffer);
        else
            fill_shent_64(&shent, buffer);

        fwrite(buffer, sizeof(uint8_t), shent_sz, file);

        shent.sh_name = get_shstr_ndx("symstr", shstr),
        shent.sh_type = SHT_STRTAB,
        shent.sh_flags = 0,
        shent.sh_addr = 0,
        shent.sh_offset = exec->aux_data.symstr_offset,
        shent.sh_size = exec->aux_data.symstr_len;
        shent.sh_link = 0,
        shent.sh_info = 0,
        shent.sh_addralign = sizeof(uint8_t);
        shent.sh_entsize = 0;

        if (format == 1)
            fill_shent_32(&shent, buffer);
        else
            fill_shent_64(&shent, buffer);

        fwrite(buffer, sizeof(uint8_t), shent_sz, file);
        section_count += 2;
    }

    Elf_shdr_t shent = {
        .sh_name = get_shstr_ndx("shstr", shstr),
        .sh_type = SHT_STRTAB,
        .sh_flags = 0,
        .sh_addr = 0,
        .sh_offset = exec->aux_data.shstr_offset,
        .sh_size = exec->aux_data.shstr_len,
        .sh_link = 0,
        .sh_info = 0,
        .sh_addralign = sizeof(uint8_t),
        .sh_entsize = 0,
    };

    if (format == 1)
        fill_shent_32(&shent, buffer);
    else
        fill_shent_64(&shent, buffer);

    fwrite(buffer, sizeof(uint8_t), shent_sz, file);
    shstrent_ndx = section_count;
    section_count++;

    buffer_ptr = 0;
    if (format == 1) {
        fseek(file, ELF_HDR_SHOFF_OFFSET_32, SEEK_SET);
        write_u32(exec->aux_data.shtab_offset, buffer, &buffer_ptr);
        fwrite(buffer, sizeof(uint8_t), sizeof(uint32_t), file);

        fseek(file, ELF_HDR_SHNUM_OFFSET_32, SEEK_SET);
        buffer_ptr = 0;
        write_u32(section_count, buffer, &buffer_ptr);
        fwrite(buffer, sizeof(uint8_t), sizeof(uint32_t), file);

        fseek(file, ELF_HDR_SHSTRNDX_OFFSET_32, SEEK_SET);
        buffer_ptr = 0;
        write_u32(shstrent_ndx, buffer, &buffer_ptr);
        fwrite(buffer, sizeof(uint8_t), sizeof(uint32_t), file);
    } else {
        fseek(file, ELF_HDR_SHOFF_OFFSET_64, SEEK_SET);
        write_u64(exec->aux_data.shtab_offset, buffer, &buffer_ptr);
        fwrite(buffer, sizeof(uint8_t), sizeof(uint64_t), file);

        fseek(file, ELF_HDR_SHNUM_OFFSET_64, SEEK_SET);
        buffer_ptr = 0;
        write_u64(section_count, buffer, &buffer_ptr);
        fwrite(buffer, sizeof(uint8_t), sizeof(uint64_t), file);

        fseek(file, ELF_HDR_SHSTRNDX_OFFSET_64, SEEK_SET);
        buffer_ptr = 0;
        write_u64(shstrent_ndx, buffer, &buffer_ptr);
        fwrite(buffer, sizeof(uint8_t), sizeof(uint64_t), file);
    }
}

static void write_phdr(FILE *file, Elf_exec_t *exec) {
    int format = exec->hints->e_ident[4], num_phent = 0;
    uint8_t buffer[PHDR_ENT_SZ_64] = { 0 }, buffer_ptr = 0;

    if (format == 1)
        fseek(file, ELF_PHOFF_32, SEEK_SET);
    else
        fseek(file, ELF_PHOFF_64, SEEK_SET);

    if (exec->text_size > 0) {
        Elf_phdr_t phent = {
            .type = PT_LOAD,
            .flags = PF_R | PF_X,
            .offset = exec->aux_data.text_offset,
            .vaddr = exec->text_vaddr,
            .paddr = exec->text_vaddr,
            .size = exec->text_size,
            .memsz = exec->text_size,
            .align = page_size,
        };

        if (format == 1)
            write_phent_32(file, &phent, buffer);
        else
            write_phent_64(file, &phent, buffer);

        num_phent++;
    }

    if (exec->data_size > 0) {
        Elf_phdr_t phent = {
            .type = PT_LOAD,
            .flags = PF_R | PF_W,
            .offset = exec->aux_data.data_offset,
            .vaddr = exec->data_vaddr,
            .paddr = exec->data_vaddr,
            .size = exec->data_size,
            .memsz = exec->data_size,
            .align = page_size,
        };

        if (format == 1)
            write_phent_32(file, &phent, buffer);
        else
            write_phent_64(file, &phent, buffer);

        num_phent++;
    }

    buffer_ptr = 0;

    if (format == 1) {
        fseek(file, ELF_HDR_PHENTSZ_OFFSET_32, SEEK_SET);
        write_u16(PHDR_ENT_SZ_32, buffer, &buffer_ptr);
    } else {
        fseek(file, ELF_HDR_PHENTSZ_OFFSET_64, SEEK_SET);
        write_u16(PHDR_ENT_SZ_64, buffer, &buffer_ptr);
    }

    write_u16(num_phent, buffer, &buffer_ptr);
    fwrite(buffer, sizeof(uint8_t), 4, file);
}

void write_elf(Elf_exec_t *exec, Elf_list *elves) {
    Elf_info_t *ptr = elves->head;
    FILE *file = fopen("a.out", "w");
    uint64_t offset = 0;

    write_elf_header(file, exec->hints, exec->aux_data.start_addr);
    write_sections(file, exec, elves->head, elves->num_files, &offset);
    write_symtab(file, exec, offset);
    write_shstr(file, exec);
    // write_missing_symtab_names(file, exec);
    write_shdr(file, exec);
    write_phdr(file, exec);

    fclose(file);
}
