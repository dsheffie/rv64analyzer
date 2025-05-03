UNAME_S = $(shell uname -s)
LIBS =  $(EXTRA_LD) -lpthread
ifeq ($(UNAME_S),Linux)
     CXX = g++ -fomit-frame-pointer
     EXTRA_LD = -lboost_program_options -lboost_serialization -lunwind -lcapstone
endif

ifeq ($(UNAME_S),Darwin)
     CXX = clang++ -fomit-frame-pointer -I/opt/local/include
     EXTRA_LD = -L/opt/local/lib -lboost_program_options-mt -lboost_serialization-mt -lcapstone 
endif


OPT = -g -O3 -Wall -Wpedantic -Wextra -Wno-unused-parameter
CXXFLAGS = -std=c++17 -g $(OPT)

EXE = perf_analyzer
OBJ = main.o cfgBasicBlock.o disassemble.o helper.o basicBlock.o compile.o riscvInstruction.o regionCFG.o githash.o
DEP = $(OBJ:.o=.d)

.PHONY: all clean

all: $(EXE)

$(EXE) : $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LIBS) -o $(EXE)

githash.cc : .git/HEAD .git/index
	echo "const char *githash = \"$(shell git rev-parse HEAD)\";" > $@

%.o: %.cc
	$(CXX) -MMD $(CXXFLAGS) -c $<

-include $(DEP)

clean:
	rm -rf $(EXE) $(OBJ) $(DEP) cfg_*
