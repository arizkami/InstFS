# Main Makefile

# This Makefile acts as a wrapper.
# It includes the correct makefile based on the operating system.
#
# For Windows, use nmake with Makefile.win:
# > nmake /f Makefile.win

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),FreeBSD)
    include Makefile.bsd
else
    # Default to linux makefile for Linux and other Unix-like systems
    include Makefile.linux
endif
