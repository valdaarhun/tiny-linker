SRC := $(wildcard *.c) $(wildcard *.S)
OBJ := $(SRC:%=%.o)
BIN := a.out
CFLAGS := -static -nostdlib -nostartfiles -fno-pic -ffreestanding
INC_FLAG = $(addprefix -I,.)

TEST_EXEC := test.sh
RETVAL = 30

ifndef LINKER
	LINKER := tiny-linker
endif

.PHONY: all
all: $(BIN)

$(BIN): $(OBJ)
	$(CURDIR)/../$(LINKER) $^

%.S.o: %.S
	$(CC) $(CFLAGS) $(INC_FLAG) -o $@ -c $^

%.c.o: %.c
	$(CC) $(CFLAGS) $(INC_FLAG) -o $@ -c $^

.PHONY: test
test: $(BIN)
	bash $(TEST_EXEC) $(BIN) $(RETVAL)

.PHONY: clean
clean:
	$(RM) $(OBJ) $(BIN)
