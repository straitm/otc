# @author Matthew Strait

CXX=g++

CPPFLAGS=-Wall -Wextra -O2
LINKFLAGS=$(CPPFLAGS)

ROOTINC = `root-config --cflags` 
LIB += -lm `root-config --libs`

all: otc

otc_obj = otc_main.o otc_root.o

otc: $(otc_obj) 
	@echo Linking otc
	@$(CXX) $(LINKFLAGS) $(LIB) -o otc $(otc_obj) $(other_obj)

otc_root.o: otc_root.cpp otc_cont.h
	@echo Compiling $<
	@$(COMPILE.cc) $(ROOTINC) $(OUTPUT_OPTION) $<

otc_main.o: otc_main.cpp otc_cont.h otc_root.h otc_progress.cpp
	@echo Compiling $<
	@$(COMPILE.cc) $(ROOTINC) $(OUTPUT_OPTION) $<

clean: 
	@rm -f otc *.o *_dict.* G__* AutoDict_* *_dict_cxx.d
