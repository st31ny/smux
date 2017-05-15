####################################################################################################
## vim:set ft=make: ################################################################################
## libsmux Makefile ################################################################################
####################################################################################################

# This Makefile compiles the smux library (libsmux).

BUILDIR                 ?= .
MYDIR                   := $(dir $(lastword $(MAKEFILE_LIST)))

## Package config
PKG                     := libsmux

TARGETNAME.$(PKG)_main  := libsmux.a

CFLAGS.$(PKG)            = -g -O2 -Wall
$(eval CFLAGS.$(PKG)    += -I"$(MYDIR)/../include")
LDFLAGS.$(PKG)           =

## Local files
SRC_C                   := smux.c

include $(BUILDIR)/mk/pkg.mk
