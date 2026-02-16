# MediaKit Root Makefile
# Builds MediaKitFoundation and all modules for Linux, Windows, and BSD

include Makefile.inc

# Modules to build
MODULES := InstFS DAUx

# Build directories
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
LIB_DIR := $(BUILD_DIR)/lib
BIN_DIR := $(BUILD_DIR)/bin

# Platform-specific settings
ifeq ($(PLATFORM),windows)
    CC := cl
    AR := lib
    CFLAGS := /W3 /O2 /EHsc /c
    ARFLAGS := /OUT:
    OBJ_EXT := .obj
    LIB_PREFIX :=
    LIB_EXT := .lib
    RM := del /Q
    RMDIR := rmdir /S /Q
    MKDIR := mkdir
else
    CC := gcc
    AR := ar
    CFLAGS := -Wall -Wextra -O2 -fPIC
    ARFLAGS := rcs
    OBJ_EXT := .o
    LIB_PREFIX := lib
    LIB_EXT := .a
    RM := rm -f
    RMDIR := rm -rf
    MKDIR := mkdir -p
endif

# MediaKitFoundation library
MKF_LIB := $(BUILD_DIR)/$(LIB_PREFIX)MediaKitFoundation$(LIB_EXT)
MKF_OBJ := $(OBJ_DIR)/MediaKitFoundation$(OBJ_EXT)

.PHONY: all foundation modules clean install help

all: foundation modules

foundation: $(MKF_LIB)

$(MKF_LIB): $(MKF_OBJ)
ifeq ($(PLATFORM),windows)
	@if not exist "$(BUILD_DIR)" $(MKDIR) "$(BUILD_DIR)"
	$(AR) $(ARFLAGS)$@ $^
	@echo Built MediaKitFoundation library: $@
else
	@$(MKDIR) $(BUILD_DIR)
	$(AR) $(ARFLAGS) $@ $^
	@echo "Built MediaKitFoundation library: $@"
endif

$(MKF_OBJ): MediaKitFoundation.c MediaKitFoundation.h
ifeq ($(PLATFORM),windows)
	@if not exist "$(OBJ_DIR)" $(MKDIR) "$(OBJ_DIR)"
	$(CC) $(CFLAGS) /Fo$@ MediaKitFoundation.c
else
	@$(MKDIR) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ MediaKitFoundation.c
endif

modules: foundation
	@echo "Building modules for $(PLATFORM)..."
	@for module in $(MODULES); do \
		if [ -d $$module ]; then \
			echo "Building $$module..."; \
			$(MAKE) -C $$module || exit 1; \
		fi; \
	done

clean:
ifeq ($(PLATFORM),windows)
	-@if exist "$(BUILD_DIR)" $(RMDIR) "$(BUILD_DIR)"
	@for %%m in ($(MODULES)) do @if exist %%m $(MAKE) -C %%m clean
else
	$(RMDIR) $(BUILD_DIR)
	@for module in $(MODULES); do \
		if [ -d $$module ]; then \
			$(MAKE) -C $$module clean; \
		fi; \
	done
endif
	@echo "Cleaned all build artifacts"

install: all
	@echo "Installing MediaKit..."
	@echo "Installation not yet implemented"

help:
	@echo "MediaKit Build System"
	@echo "====================="
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build foundation and all modules (default)"
	@echo "  foundation - Build MediaKitFoundation library only"
	@echo "  modules    - Build all modules"
	@echo "  clean      - Remove all build artifacts"
	@echo "  install    - Install libraries and headers"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Platform: $(PLATFORM)"
	@echo "Modules: $(MODULES)"
	@echo ""
	@echo "Usage:"
	@echo "  make              # Build everything"
	@echo "  make clean        # Clean everything"
	@echo "  make foundation   # Build foundation only"
