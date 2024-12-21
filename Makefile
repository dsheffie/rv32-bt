UNAME_S = $(shell uname -s)
LIBS =  $(EXTRA_LD) -lpthread -lffi -lcurses -lz
ifeq ($(UNAME_S),Linux)
     LLVM_CXXFLAGS = $(shell llvm-config-14 --cppflags)
     LLVM_LDFLAGS = $(shell llvm-config-14 --ldflags --libs all)
     CXX = clang++ -fomit-frame-pointer -flto
     EXTRA_LD = -ldl -lffi -lbfd -lboost_program_options -lunwind -lcapstone
     DL = -Wl,--export-dynamic 
endif

ifeq ($(UNAME_S),FreeBSD)
     LLVM_CXXFLAGS = $(shell llvm-config10 --cppflags)
     LLVM_LDFLAGS = $(shell llvm-config10 --ldflags --libs all)
     CXX = clang++ -march=native -fomit-frame-pointer -flto
     EXTRA_LD = -L/usr/local/lib -lboost_program_options  -lunwind -lcapstone
     DL = -Wl,--export-dynamic 
endif

ifeq ($(UNAME_S),Darwin)
     LLVM_CXXFLAGS = $(shell llvm-config-mp-14 --cppflags)
     LLVM_LDFLAGS = $(shell llvm-config-mp-14 --ldflags --libs all)
     CXX = clang++ -fomit-frame-pointer -I/opt/local/include
     EXTRA_LD = -L/opt/local/lib -lboost_program_options-mt -lcapstone
endif


CXXFLAGS = -std=c++17 -g $(OPT) $(LLVM_CXXFLAGS)


OPT = -g -O3 -Wall -Wpedantic -Wextra -Wno-unused-parameter -ferror-limit=1
EXE = cfg_rv32
OBJ = main.o cfgBasicBlock.o loadelf.o disassemble.o helper.o interpret.o basicBlock.o compile.o region.o riscvInstruction.o regionCFG.o perfmap.o debugSymbols.o saveState.o simPoints.o githash.o state.o
DEP = $(OBJ:.o=.d)

.PHONY: all clean

all: $(EXE)

$(EXE) : $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LLVM_LDFLAGS) $(LIBS) $(DL) -o $(EXE)

githash.cc : .git/HEAD .git/index
	echo "const char *githash = \"$(shell git rev-parse HEAD)\";" > $@

%.o: %.cc
	$(CXX) -MMD $(CXXFLAGS) -c $<

-include $(DEP)

clean:
	rm -rf $(EXE) $(OBJ) $(DEP) cfg_*
