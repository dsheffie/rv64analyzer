#include <cstdint>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cassert>
#include <fstream>
#include <boost/program_options.hpp>

#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <fenv.h>
#include <setjmp.h>

#include "helper.hh"
#include "disassemble.hh"
#include "interpret.hh"
#include "basicBlock.hh"
#include "region.hh"
#include "regionCFG.hh"
#include "perfmap.hh"
#include "debugSymbols.hh"
#include "globals.hh"
#include "simPoints.hh"

extern const char* githash;
int sArgc = -1;
char** sArgv = nullptr;

namespace globals {
  int sArgc = 0;
  char** sArgv = nullptr;
  bool isMipsEL = false;
  llvm::CodeGenOpt::Level regionOptLevel = llvm::CodeGenOpt::Aggressive;
  bool countInsns = true;
  bool simPoints = false;
  bool replay = false;
  uint64_t simPointsSlice = 0;
  uint64_t nCfgCompiles = 0;
  region *regionFinder = nullptr;
  basicBlock *cBB = nullptr;
  execUnit *currUnit = nullptr;
  bool enClockFuncts = false;
  bool enableCFG = true;
  bool verbose = false;
  bool ipo = true;
  bool fuseCFGs = true;
  bool enableBoth = true;
  uint32_t enoughRegions = 5;
  bool dumpIR = false;
  bool dumpCFG = false;
  bool splitCFGBBs = true;
  uint64_t nFuses = 0;
  uint64_t nAttemptedFuses = 0;
  std::string blobName;
  uint64_t icountMIPS = 500;
  cfgAugEnum cfgAug = cfgAugEnum::none;
  std::string binaryName;
  std::set<int> openFileDes;
  bool profile = false;
  uint64_t dumpicnt = ~(0UL);
  bool log = false;
  std::map<std::string, uint32_t> symtab;
  uint64_t tohost_addr = 0;
  uint64_t fromhost_addr = 0;
  std::map<uint32_t, uint64_t> syscall_histo;
}

perfmap* perfmap::theInstance = nullptr;
std::set<regionCFG*> regionCFG::regionCFGs;
uint64_t regionCFG::icnt = 0;
uint64_t regionCFG::iters = 0;
std::map<uint32_t, basicBlock*> basicBlock::bbMap;
std::map<uint32_t, basicBlock*> basicBlock::insMap;
std::map<uint32_t, uint64_t> basicBlock::insInBBCnt;


int main(int argc, char *argv[]) {
  initCapstone();
  
  stopCapstone();

  return 0;
}


