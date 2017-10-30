## Makefile subdir
# vim:set ft=make:

MYDIR                   := $(dir $(lastword $(MAKEFILE_LIST)))

SRC_CXX                 := file_factory.cpp files.cpp rt.cpp cnf.cpp cnf_argv.cpp \
                           debug.cpp
SRC_CXX_main            := main.cpp
SRC_CXX_test            := test_dummy.cpp

include $(BUILDIR)/mk/dir.mk
