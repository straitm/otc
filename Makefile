# @author Matthew Strait

CXX=g++

CPPFLAGS=-Wall -Wextra -O2
LINKFLAGS=$(CPPFLAGS)

DOGSINC = -I${DOGS_PATH}/DCReco/RecoOV
ROOTINC = `root-config --cflags` 
LIB += -lm `root-config --libs`

all: otc

otc_obj = otc_main.o otc_root.o

other_obj += ${DOGS_PATH}/DCBase/DC{Message,SimFlag,Detector}.o
other_obj += ${DOGS_PATH}/DCValidity/*.o
other_obj += ${DOGS_PATH}/DCGeo/*.o
other_obj += ${DOGS_PATH}/DCDB/DC{Param,Table}.o

otc: $(otc_obj) 
	@echo Linking otc
	@$(CXX) $(LINKFLAGS) $(LIB) -o otc $(otc_obj) $(other_obj)

otc_root.o: otc_root.cpp otc_cont.h
	@echo Compiling $<
	@$(COMPILE.cc) $(DOGSINC) $(ROOTINC) $(OUTPUT_OPTION) $<

otc_main.o: otc_main.cpp otc_cont.h otc_root.h otc_progress.cpp
	@echo Compiling $<
	@$(COMPILE.cc) $(ROOTINC) $(DOGSINC) $(OUTPUT_OPTION) $<

clean: 
	@rm -f otc *.o *_dict.* G__* AutoDict_* *_dict_cxx.d
