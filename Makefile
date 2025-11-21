
#
# Variables
#

# Fixed directories/args
TARGET   := hmc
SRC_DIR  := ./src/
LIB_DIR  := ./lib/
REL_DIR  := ./build/release/
DBG_DIR  := ./build/debug/

INC_DIRS  := $(shell find $(LIB_DIR) -type d) $(shell find $(SRC_DIR) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11
CPPFLAGS := $(INC_FLAGS) -MMD -MP

# Need to link against math.h manually
LFLAGS   := -lm
# ----------------------

# Dependent directories/args
SRCS  = $(shell find $(SRC_DIR) -name '*.c')
LIBS  = $(shell find $(LIB_DIR) -name '*.c')
OBJS  = $(SRCS:%=$(BLD_DIR)/%.o)
OBJS += $(LIBS:%=$(BLD_DIR)/%.o)
DEPS  = $(OBJS:.o=.d)
EXE   = $(BLD_DIR)/$(TARGET)
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
	mkdir -p $(DBG_DIR) $(REL_DIR) $(addprefix $(DBG_DIR),$(INC_DIRS)) $(addprefix $(REL_DIR),$(INC_DIRS))

clean:
	rm -rf $(DBG_DIR)/$(TARGET) $(REL_DIR)/$(TARGET) $(addprefix $(DBG_DIR),$(INC_DIRS)) $(addprefix $(REL_DIR),$(INC_DIRS))

-include $(DEPS)
