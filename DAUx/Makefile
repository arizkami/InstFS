# DAUx Makefile - Direct Audio Engine
# Cross-platform: Linux (ALSA), Windows (WASAPI), BSD (OSS)

# Default target must be first
.PHONY: all clean
all:

# Include common configuration
include ../Makefile.inc
include ../share/mk/mk.mkf.module

# Module information
MODULE_NAME := DAUx
VERSION := 1.0.0

# Directories
SRC_DIR := src
INC_DIR := include

# Source files
C_SOURCES := $(SRC_DIR)/DAUx.c $(SRC_DIR)/stream.c
ASM_SOURCES := $(SRC_DIR)/accel.S

# Object files
C_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst $(SRC_DIR)/%.S,$(OBJ_DIR)/%.o,$(ASM_SOURCES))
ALL_OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)

# Output library
ifeq ($(PLATFORM),windows)
    LIBRARY := $(LIB_DIR)/$(MODULE_NAME)$(LIB_EXT)
    CFLAGS += /I$(INC_DIR)
    PLATFORM_LIBS := Ole32.lib
else
    LIBRARY := $(LIB_DIR)/$(LIB_PREFIX)$(MODULE_NAME)$(LIB_EXT)
    CFLAGS += -I$(INC_DIR)
    
    ifeq ($(PLATFORM),linux)
        PLATFORM_LIBS := -lasound -lpthread
    else ifeq ($(PLATFORM),bsd)
        PLATFORM_LIBS := -lpthread
    endif
endif

# Redefine all target with actual dependency
all: $(LIBRARY)

$(LIBRARY): $(ALL_OBJECTS)
ifeq ($(PLATFORM),windows)
	@if not exist "$(LIB_DIR)" $(MKDIR) "$(LIB_DIR)"
	$(AR) $(ARFLAGS)$@ $(ALL_OBJECTS)
	@echo Built $(MODULE_NAME) library: $@
else
	@$(MKDIR) $(LIB_DIR)
	$(AR) $(ARFLAGS) $@ $(ALL_OBJECTS)
	@echo "Built $(MODULE_NAME) library: $@"
endif

# Compile C sources
ifeq ($(PLATFORM),windows)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@if not exist "$(OBJ_DIR)" $(MKDIR) "$(OBJ_DIR)"
	$(CC) $(CFLAGS) /c /Fo$@ $<
else
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@$(MKDIR) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<
endif

# Assemble ASM sources
ifeq ($(PLATFORM),windows)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S
	@if not exist "$(OBJ_DIR)" $(MKDIR) "$(OBJ_DIR)"
	$(AS) $(ASFLAGS) -o $@ $<
else
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S
	@$(MKDIR) $(OBJ_DIR)
	$(AS) $(ASFLAGS) -o $@ $<
endif

# Dependencies
$(C_OBJECTS): $(INC_DIR)/DAUx.h $(INC_DIR)/stream.h
