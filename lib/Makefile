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

CFLAGS.$(PKG)            = -g -O2 -Wall -Wextra
$(eval CFLAGS.$(PKG)    += -I"$(MYDIR)/../include")
$(eval CXXFLAGS.$(PKG)  += -I"$(MYDIR)/../include")
LDFLAGS.$(PKG)           =
LDLIBS.$(PKG)_test       = -lboost_unit_test_framework

## Local files
SRC_C                   := smux.c

SUBDIRS                 := test

include $(BUILDIR)/mk/pkg.mk
