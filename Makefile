####################################################################################################
## vim:set ft=make: ################################################################################
## smux app Makefile ###############################################################################
####################################################################################################

# This Makefile compiles the linux application smux AND the smux library (libsmux).

BUILDIR                 ?= .
MYDIR                   := $(dir $(lastword $(MAKEFILE_LIST)))

## Package config
PKG                     := smux

CXXFLAGS.$(PKG)          = -g -O2 -Wall -Wextra -std=c++11
LDFLAGS.$(PKG)           =

# smux lib
$(eval CXXFLAGS.$(PKG)  += -I"$(MYDIR)/include")
LDFLAGS.$(PKG)          += -L$(BUILDIR)/bin/main/libsmux
LDLIBS.$(PKG)           += -lsmux

## Dependencies
DEPENDS.$(PKG)          += libsmux

## Recursion
SUBDIRS                 := src
PKGDIRS                 := lib

include $(BUILDIR)/mk/pkg.mk
