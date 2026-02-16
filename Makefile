# Main Makefile

UNAME_S != uname -s

# This Makefile acts as a wrapper.
# It includes the correct makefile based on the operating system.
#
# For Windows, use nmake with Makefile.win:
# > nmake /f Makefile.win

.if $(UNAME_S:MFreeBSD)
.include "Makefile.bsd"
.else
.include "Makefile.linux"
.endif
