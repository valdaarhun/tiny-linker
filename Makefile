SRC_DIR := ./src
BUILD_DIR := ./build
INC_DIR := $(SRC_DIR)

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
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

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	$(RM) $(BIN)
	$(RM) -r $(BUILD_DIR)

-include $(DEP)
