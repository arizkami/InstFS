# Makefile.inc - Common configuration for all modules

# Project information
PROJECT_NAME := MediaKit
VERSION := 1.0.0

# Root directory
ROOT_DIR := $(shell pwd)

# Build configuration
BUILD_TYPE ?= release
VERBOSE ?= 0

# Platform detection
ifndef PLATFORM
    ifeq ($(OS),Windows_NT)
        PLATFORM := windows
    else
        UNAME_S := $(shell uname -s)
        ifeq ($(UNAME_S),Linux)
            PLATFORM := linux
        else ifeq ($(UNAME_S),FreeBSD)
            PLATFORM := bsd
        else ifeq ($(UNAME_S),OpenBSD)
            PLATFORM := bsd
        else ifeq ($(UNAME_S),NetBSD)
            PLATFORM := bsd
        else
            PLATFORM := posix
        endif
    endif
endif

# Export variables for sub-makes
export PLATFORM
export ROOT_DIR
export BUILD_TYPE
export VERBOSE
