## Makefile subdir
# vim:set ft=make:

MYDIR                   := $(dir $(lastword $(MAKEFILE_LIST)))

SRC_CXX                 := main.cpp

include $(BUILDIR)/mk/dir.mk
