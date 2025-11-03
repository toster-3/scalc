MODE     := dev
ifeq ($(MODE), dev)
	OFLAGS := -g -O0 -DDEBUG
else
	OFLAGS := -O3 -march=native
endif
CPPFLAGS :=
CFLAGS   := -std=c11 -pedantic -Wall $(OFLAGS)
LDFLAGS  := -lc

CC = cc

TARGET_EXE := scalc

BUILD_DIR := build
SRC_DIR   := src
INC_DIR   := src

CPPFLAGS += $(addprefix -I,$(INC_DIR))

SRC := $(shell find $(SRC_DIR) -name '*.c')
OBJ := $(addprefix $(BUILD_DIR)/, $(notdir $(SRC:.c=.o)))

all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET_EXE)

$(BUILD_DIR)/$(TARGET_EXE): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) -fsanitize=address

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -r $(BUILD_DIR)

.PHONY: all clean
