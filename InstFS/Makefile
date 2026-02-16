# Main Makefile

# This Makefile acts as a wrapper.
# It includes the correct makefile based on the operating system.
#
# For Windows, use nmake with Makefile.win:
# > nmake /f Makefile.win

SHELL := /bin/sh

include $(shell test "`uname -s`" = "FreeBSD" && echo "Makefile.bsd" || echo "Makefile.linux")
