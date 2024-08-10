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
#include "regionCFG.hh"
#include "debugSymbols.hh"
#include "globals.hh"

extern const char* githash;
int sArgc = -1;
char** sArgv = nullptr;

namespace globals {
  int sArgc = 0;
  char** sArgv = nullptr;
  bool isMipsEL = false;
  bool countInsns = true;
  bool simPoints = false;
  bool replay = false;
  uint64_t simPointsSlice = 0;
  uint64_t nCfgCompiles = 0;
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
  std::string binaryName;
  std::set<int> openFileDes;
  bool profile = false;
  uint64_t dumpicnt = ~(0UL);
  bool log = false;
  uint64_t tohost_addr = 0;
  uint64_t fromhost_addr = 0;
  std::map<uint32_t, uint64_t> syscall_histo;
}

std::set<regionCFG*> regionCFG::regionCFGs;
uint64_t regionCFG::icnt = 0;
uint64_t regionCFG::iters = 0;
std::map<uint64_t, basicBlock*> basicBlock::bbMap;
std::map<uint64_t, basicBlock*> basicBlock::insMap;


int main(int argc, char *argv[]) {
  retire_trace rt;
  initCapstone();
  std::ifstream ifs("test.dump", std::ios::binary);
  boost::archive::binary_iarchive ia(ifs);  
  ia >> rt;

  std::cout << "rt.get_records().size() = " <<
    rt.get_records().size() << "\n";


  globals::cBB = new basicBlock(rt.get_records().begin()->pc);
  buildCFG(rt.get_records());

  std::ofstream out("blocks.txt");
  std::vector<std::vector<basicBlock*>> regions;
  std::vector<basicBlock*> r;

  for(auto p : basicBlock::bbMap) {
    out << *(p.second) << "\n";
    r.push_back(p.second);
  }
  
  regions.push_back(r);
  regionCFG *cfg = new regionCFG();
  cfg->buildCFG(regions);
  stopCapstone();

  return 0;
}


