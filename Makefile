
#
# Variables
#

# Fixed directories/args
TARGET   := hmc
SRC_DIR  := ./src
REL_DIR  := ./build/release
DBG_DIR  := ./build/debug

INC_DIRS  := $(shell find $(SRC_DIR) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11
CPPFLAGS := $(INC_FLAGS) -MMD -MP

# Need to link against math.h manually
LFLAGS   := -lm
# ----------------------

# Dependent directories/args
SRCS = $(shell find $(SRC_DIR) -name '*.c')
OBJS = $(SRCS:%=$(BLD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)
EXE  = $(BLD_DIR)/$(TARGET)
# --------------------------


#
# TARGETS
#

.PHONY: all debug release clean prep


all: prep clean debug release


debug: BLD_DIR = $(DBG_DIR)
debug: CFLAGS += -g -Og -DDEBUG
debug: $(EXE)


release: BLD_DIR = $(REL_DIR)
release: CFLAGS += -O3 -DRELEASE
release: $(EXE)

$(EXE): $(OBJS)
	$(CC) $(OBJS) -o $(EXE) $(LFLAGS)

$(OBJS): $(BUILD_DIR)/%.c.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $(BLD_DIR)$@


prep:
	mkdir -p $(DBG_DIR) $(DBG_DIR)/$(SRC_DIR) $(REL_DIR) $(REL_DIR)/$(SRC_DIR)

clean:
	rm -rf $(DBG_DIR)/$(TARGET) $(REL_DIR)/$(TARGET) $(DBG_DIR)/$(SRC_DIR)/* $(REL_DIR)/$(SRC_DIR)/*

-include $(DEPS)
