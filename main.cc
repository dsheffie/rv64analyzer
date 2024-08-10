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

static void getNextBlock(uint64_t pc) {
  basicBlock *nBB = globals::cBB->findBlock(pc);
  if(nBB == nullptr ) {
    nBB = new basicBlock(pc, globals::cBB);
  }
  globals::cBB->setReadOnly();
  globals::cBB = nBB;
}

void execRiscv(uint32_t inst, uint64_t pc, uint64_t npc, uint64_t vpc) {
  globals::cBB->addIns(inst, pc, vpc);
  uint32_t opcode = inst & 127;
  switch(opcode)
    {
      //imm[11:0] rs1 000 rd 1100111 JALR
    case 0x67: {
      globals::cBB->setTermAddr(pc);      
      getNextBlock(npc);
      break;
    }
      //imm[20|10:1|11|19:12] rd 1101111 JAL
    case 0x6f: {
      globals::cBB->setTermAddr(pc);      
      getNextBlock(npc);
      break;
    }
    case 0x63: { /* cond branch */
      globals::cBB->setTermAddr(pc);
      getNextBlock(npc);
      break;
    }
    default:
      break;
    }
}


void buildCFG(const std::list<inst_record> &trace, std::map<uint64_t,uint64_t> &counts) {
  auto nit = trace.begin(); nit++;
  uint64_t cnt = 0;
  for(auto it = trace.begin(), E = trace.end(); nit != E; ++it) {
    uint64_t npc = ~0UL;
    const inst_record & ir = *it;
    if(nit != E) {
      npc = nit->pc;
    }
    counts[ir.pc]++;
#if 0
    printf("%lx %s -> %lx (cbb %lx, term %lx, read only %d)\n",
	   ir.pc,
	   getAsmString(ir.inst, ir.pc).c_str(),
	   npc,
	   globals::cBB->getEntryAddr(),
	   globals::cBB->getTermAddr(),
	   globals::cBB->isReadOnly()
	   );
#endif
    
    if( not(globals::cBB->isReadOnly()) ) {
      execRiscv(ir.inst, ir.pc, npc, ir.vpc);
    }
    else if(ir.pc == globals::cBB->getTermAddr()) {
      auto nbb = globals::cBB->findBlock(npc);
      if(nbb == nullptr)  {
	nbb = new basicBlock(npc, globals::cBB);
      }
      globals::cBB = nbb;
    }
  next:
    ++cnt;
    ++nit;
  }
  std::cout << "made it to end of trace\n";
}




int main(int argc, char *argv[]) {
  retire_trace rt;
  tip_record tip;
  initCapstone();
  std::map<uint64_t,uint64_t> counts;
  std::ifstream trace_ifs("test.dump", std::ios::binary);
  boost::archive::binary_iarchive rt_(trace_ifs);
  std::ifstream tip_ifs("tip.dump", std::ios::binary);
  boost::archive::binary_iarchive tip_(tip_ifs);  
  
  rt_ >> rt;
  tip_ >> tip;


  std::cout << "rt.get_records().size() = " <<
    rt.get_records().size() << "\n";


  globals::cBB = new basicBlock(rt.get_records().begin()->pc);
  buildCFG(rt.get_records(), counts);

  std::ofstream out("blocks.txt");
  std::vector<std::vector<basicBlock*>> regions;
  std::vector<basicBlock*> r;

  //create region
  for(auto p : basicBlock::bbMap) {
    out << *(p.second) << "\n";
    r.push_back(p.second);
  }
  
  regions.push_back(r);
  regionCFG *cfg = new regionCFG(tip.m, counts);
  cfg->buildCFG(regions);
  stopCapstone();

  return 0;
}


