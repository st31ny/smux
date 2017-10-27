## Makefile template for subdirs
# vim:set ft=make:

MYDIR                   := $(dir $(lastword $(MAKEFILE_LIST)))

SRC_CXX_test            := test.cpp read_decode_test.cpp

include $(BUILDIR)/mk/dir.mk
