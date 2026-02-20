# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -O3 -ffast-math -Iinclude -MMD -MP $(shell sdl2-config --cflags)
LDFLAGS := $(shell sdl2-config --libs)

# Directories
SRC_DIR := src
OUT_DIR := out

# Output name
NAME := renderer
TARGET := $(OUT_DIR)/$(NAME)

# Source and object files
SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OUT_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# Debug flags (For lldb/address sanitizer)
DEBUG_CFLAGS := -g -O0 -fsanitize=address
DEBUG_LDFLAGS := -fsanitize=address

# Profiling flags (For Instruments - Optimized + Symbols)
# -fno-omit-frame-pointer is essential for Instruments to walk the stack
PROFILE_CFLAGS := -g -O3 -fno-omit-frame-pointer
PROFILE_LDFLAGS := -g

# Default targets
.PHONY: all clean run debug profile run-debug run-profile

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OUT_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

# --- Execution Targets ---

# Run normally
run: all
	./$(TARGET)

# Debug build (Slow, but catches memory errors)
debug: CFLAGS += $(DEBUG_CFLAGS)
debug: LDFLAGS += $(DEBUG_LDFLAGS)
debug: clean all

run-debug: debug
	./$(TARGET)

# Profile build (Fast, but keeps function names for Instruments)
profile: CFLAGS += $(PROFILE_CFLAGS)
profile: LDFLAGS += $(PROFILE_LDFLAGS)
profile: clean all

run-profile: profile
	./$(TARGET)

# Clean everything
clean:
	rm -rf $(OUT_DIR)