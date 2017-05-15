####################################################################################################
## vim:set ft=make: ################################################################################
## ADS Build System ################################################################################
## Copyright (C) 2017 Maximilian Stein #############################################################
####################################################################################################
## Collect Makefile Fragment #######################################################################
## Aggregate local source files ####################################################################
####################################################################################################

## Build list of local object files
# evaluate rhs immediately and dirctly merge build-specific files
# unset local variables (avoid side effects when unused)

# c++
__SRC_CXX               := $(addprefix $(MYDIR)/,$(SRC_CXX)) $(addprefix $(MYDIR)/,$(SRC_CXX_$(BUILD)))
__OBJ_CXX               := $(patsubst %,$(OBJDIR.$(PKG))/cxx/%.o,$(__SRC_CXX))
SRC_CXX                 :=
SRC_CXX_$(BUILD)        :=

# c
__SRC_C                 := $(addprefix $(MYDIR)/,$(SRC_C)) $(addprefix $(MYDIR)/,$(SRC_C_$(BUILD)))
__OBJ_C                 := $(patsubst %,$(OBJDIR.$(PKG))/c/%.o,$(__SRC_C))
SRC_C                   :=
SRC_C_$(BUILD)          :=

## Local object files
__OBJ                   := $(__OBJ_CXX) $(__OBJ_C)
