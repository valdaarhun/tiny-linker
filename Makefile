SRC_DIR := ./src
BUILD_DIR := ./build
TEST_DIR := ./test
INC_DIR := $(SRC_DIR)

SRC := $(shell find $(SRC_DIR) -name "*.c")
OBJ := $(SRC:$(SRC_DIR)/%=$(BUILD_DIR)/%.o)
DEP := $(OBJ:.o=.d)
BIN := tiny-linker

INC_FLAG := $(addprefix -I,$(INC_DIR))
CFLAGS := -Wall -MMD -MP $(INC_FLAG)

ifdef DEBUG
	ifneq ($(DEBUG), 0)
		CFLAGS := $(CFLAGS) -DDEBUG
	endif
endif

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: test
test:
	$(MAKE) -C $(TEST_DIR)

.PHONY: clean
clean:
	$(RM) $(BIN)
	$(RM) -r $(BUILD_DIR)
	$(MAKE) -C $(TEST_DIR) clean

-include $(DEP)
