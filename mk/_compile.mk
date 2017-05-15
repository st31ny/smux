####################################################################################################
## vim:set ft=make: ################################################################################
## ADS Build System ################################################################################
## Copyright (C) 2017 Maximilian Stein #############################################################
####################################################################################################
## Compile Makefile Fragment #######################################################################
## Define targets for compilation and linking ######################################################
####################################################################################################

## Link
# link with g++ to get c++ libraries right
LD                      := g++

LDFLAGS.$(PKG)          += $(LDFLAGS.$(PKG)_$(BUILD))
LDLIBS.$(PKG)           += $(LDLIBS.$(PKG)_$(BUILD))

# link program
$(TARGETDIR.$(PKG))/%: $$(OBJ.$(PKG))
	@echo " >> (ld)   link  [$@]"
	@$(MKDIR) $(dir $(TARGET.$(PKG)))
	@$(LD) -o $@ $(LDFLAGS.$(PKG)) $(OBJ.$(PKG)) $(LDLIBS.$(PKG))

# link library
$(TARGETDIR.$(PKG))/%.a: $$(OBJ.$(PKG))
	@echo " >> (ar)   link  [$@]"
	@$(MKDIR) $(dir $(TARGET.$(PKG)))
	@$(AR) rcs $@ $(OBJ.$(PKG))

## Compile
# c++
CXXFLAGS.$(PKG)         += $(CXXFLAGS.$(PKG)_$(BUILD))

$(OBJDIR.$(PKG))/cxx/%.o: %
	@echo " >> (c++)  build [$@] from [$<]"
	@$(MKDIR) $(dir $@)
	@$(CXX) $(CXXFLAGS.$(PKG)) -c -MD -MF $@.d -o $@ $<

# c
CFLAGS.$(PKG)           += $(CFLAGS.$(PKG)_$(BUILD))

$(OBJDIR.$(PKG))/c/%.o: %
	@echo " >> (c)    build [$@] from [$<]"
	@$(MKDIR) $(dir $@)
	@$(CC) $(CFLAGS.$(PKG)) -c -MD -MF $@.d -o $@ $<
