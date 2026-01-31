#include <stdio.h>

#include "elf.h"

int main(int argc, char* argv[]) {
  FILE *f = fopen(argv[1], "r");
  read_elf(f);
}

