#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "elf/elf.h"

static void assign_section_vaddr(Elf_list *elves, Elf_exec_t *exec) {
    int page_size = getpagesize();
    Elf_info_t *ptr = elves->head;

    exec->text_vaddr = START_ADDR;

    for(int i = 0; i < elves->num_files; i++) {
        printf("%x\n", ptr->text.size);
        exec->text_size += ptr->text.size;
        ptr = ptr->next;
    }

    /*
     * Do the same thing for other sections (e.g. ".data"). Ensure the section starts
     * at a page boundary.
     */
    exec->data_vaddr = exec->text_vaddr + ((exec->text_size + page_size - 1) / page_size) * page_size;
    // exec->data_vaddr = exec->text_vaddr + exec->text_size;
    ptr = elves->head;

    for(int i = 0; i < elves->num_files; i++) {
        exec->data_size += ptr->data.size;
        ptr = ptr->next;
    }

    printf("text -> 0x%lx of len 0x%x\n", exec->text_vaddr, exec->text_size);
    printf("text -> 0x%lx of len 0x%x\n", exec->data_vaddr, exec->data_size);
}

int main(int argc, char* argv[]) {
    Elf_list elves;
    Elf_exec_t exec;
    Elf_info_t *elf_info, *ptr = NULL;
    uint32_t vaddr_curr =  0x400000;
    elves.num_files = argc - 1;

    for(int i = 1; i <= elves.num_files; i++) {
        FILE *f = fopen(argv[i], "r");
        elf_info = read_elf(f);
        if (!elf_info) {
            fprintf(stderr, "Error linking file %s\n", argv[i]);
            // cleanup();
            return -1;
        }

        if (i == 1) {
            elves.head = elf_info;
            ptr = elf_info;
        } else {
            ptr->next = elf_info;
            ptr = ptr->next;
        }
#ifdef DEBUG
        uint64_t len = elf_info->text.size;
        printf(".text size -> %lu\n", len);
        printf(".text name -> %s\n", &elf_info->shstr[elf_info->text.name]);
        fseek(f, elf_info->text.offset, SEEK_SET);
        unsigned char buffer[len];
        fgets(buffer, len + 1, f);
        for(int j = 0; j < len; j++) {
            printf("%x ", buffer[j]);
        }
        printf("\n");
#endif
    }

    assign_section_vaddr(&elves, &exec);
}
