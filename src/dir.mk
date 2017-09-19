## Makefile subdir
# vim:set ft=make:

MYDIR                   := $(dir $(lastword $(MAKEFILE_LIST)))

SRC_CXX                 := main.cpp file_factory.cpp files.cpp rt.cpp cnf.cpp

include $(BUILDIR)/mk/dir.mk
