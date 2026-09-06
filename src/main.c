#include <stdint.h>
#include <stdio.h>

#include "link.h"

int main(int argc, char* argv[]) {
    Elf_list elves = { 0 };
    Elf_exec_t exec = { 0 };
    Elf_info_t *elf_info, *ptr = NULL;
    elves.num_files = argc - 1;

    if (elves.num_files == 0) {
        fprintf(stderr, "No input files fed to linker\n");
        return -1;
    }

    for(int i = 1; i <= elves.num_files; i++) {
        FILE *f = fopen(argv[i], "r");

        if (!f) {
            fprintf(stderr, "Couldn't find '%s'\n", argv[i]);
            return -1;
        }

        elf_info = read_elf(f);
        if (!elf_info) {
            fprintf(stderr, "Error linking file %s\n", argv[i]);
            // cleanup();
            return -1;
        }

        if (i == 1) {
            elves.head = elf_info;
        } else {
            ptr->next = elf_info;
        }

        ptr = elf_info;
#ifdef DEBUG
        uint64_t len = elf_info->text.size;
        printf(".text size -> %lu\n", len);
        printf(".text name -> %s\n", &elf_info->shstr[elf_info->text.name]);
        fseek(f, elf_info->text.offset, SEEK_SET);
        unsigned char buffer[len];
        fgets((char *)buffer, len + 1, f);
        for(int j = 0; j < len; j++) {
            printf("%x ", buffer[j]);
        }
        printf("\n");
#endif
    }

    if (resolve_symbols(&elves, &exec) < 0) {
        return -1;
    }

    assign_vaddr(&elves, &exec);
}
