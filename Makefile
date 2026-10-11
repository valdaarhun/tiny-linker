SRC_DIR := ./src
BUILD_DIR := ./build
TEST_DIR := ./test
INC_DIR := $(SRC_DIR)
CRT := $(SRC_DIR)/crt0.S
CRT_OBJ := $(BUILD_DIR)/$(shell basename $(CRT)).o

SRC := $(shell find $(SRC_DIR) -name "*.c")
OBJ := $(SRC:$(SRC_DIR)/%=$(BUILD_DIR)/%.o)
DEP := $(OBJ:.o=.d)
BIN := tiny-linker
TMP := /tmp/$(BIN)

INC_FLAG := $(addprefix -I,$(INC_DIR))
CFLAGS := -Wall -MMD -MP $(INC_FLAG)
CFLAGS_CRT := -static -nostdlib -nostartfiles -fno-pic -ffreestanding

ifdef DEBUG
	ifneq ($(DEBUG), 0)
		CFLAGS := $(CFLAGS) -DDEBUG
	endif
endif

.PHONY: all
all: $(BIN) $(CRT_OBJ)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ -c $^

$(CRT_OBJ): $(CRT)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ -c $^
	mkdir -p $(TMP)
	cp $(CRT_OBJ) $(TMP)

.PHONY: test
test: $(BIN) $(CRT_OBJ)
	$(MAKE) LINKER=$(BIN) CRT="$(TMP)/$(shell basename $(CRT_OBJ))" -C $(TEST_DIR) test

.PHONY: clean
clean:
	$(RM) $(BIN)
	$(RM) -r $(BUILD_DIR)
	$(RM) -r $(TMP)
	$(MAKE) -C $(TEST_DIR) clean

-include $(DEP)
