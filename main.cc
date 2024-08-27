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
#include "basicBlock.hh"
#include "regionCFG.hh"
#include "globals.hh"
#include "inst_record.hh"
#include "pipeline_record.hh"

extern const char* githash;

namespace globals {
  basicBlock *cBB = nullptr;
  execUnit *currUnit = nullptr;
  bool enableCFG = true;
  bool verbose = false;
  bool dumpIR = false;
  bool dumpCFG = false;
}
std::map<uint64_t, std::map<uint64_t, uint64_t>> basicBlock::globalEdges;
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
  for(auto it = trace.begin(), E = trace.end(); nit != E; ++it) {
    uint64_t npc = ~0UL;
    const inst_record & ir = *it;
    if(nit != E) {
      npc = nit->pc;
      basicBlock::globalEdges[ir.pc][npc]++;
    }
    counts[ir.pc]++;
    // if(ir.pc == 0x8a7de5ecUL) {
    //   printf("%lx %s -> %lx (cbb %lx, term %lx, read only %d)\n",
    // 	     ir.pc,
    // 	     getAsmString(ir.inst, ir.pc).c_str(),
    // 	     npc,
    // 	     globals::cBB->getEntryAddr(),
    // 	     globals::cBB->getTermAddr(),
    // 	     globals::cBB->isReadOnly()
    // 	     );
    // }
    
    if( not(globals::cBB->isReadOnly()) ) {
      if(basicBlock::bbInBlock(ir.pc) != nullptr) {
	std::cout << *(globals::cBB);

	auto &ic = globals::cBB->getVecIns();
	auto lpc = ic.at(ic.size()-1).pc;
	globals::cBB->setTermAddr(lpc);      
	getNextBlock(ir.pc);	
	
	printf("%lx %s -> %lx (cbb %lx, term %lx, read only %d)\n",
	       ir.pc,
	       getAsmString(ir.inst, ir.pc).c_str(),
	       npc,
	       globals::cBB->getEntryAddr(),
	       globals::cBB->getTermAddr(),
	       globals::cBB->isReadOnly()
	       );
	//abort();
      }
      execRiscv(ir.inst, ir.pc, npc, ir.vpc);
    }
    else if(ir.pc == globals::cBB->getTermAddr()) {
      auto nbb = globals::cBB->findBlock(npc);
      if(nbb == nullptr)  {
	nbb = new basicBlock(npc, globals::cBB);
      }
      globals::cBB = nbb;
    }
    ++nit;
  }
}




int main(int argc, char *argv[]) {
  namespace po = boost::program_options; 
  retire_trace rt;
  pipeline_reader pt;
  std::string input, pipe;
  std::map<uint64_t,uint64_t> counts;
  try {
    po::options_description desc("Options");
    desc.add_options() 
      ("help", "Print help messages")
      ("in,i", po::value<std::string>(&input), "input dump")
      ("pipe,p", po::value<std::string>(&pipe), "pipe dump")
     
      ; 
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 
  }
  catch(po::error &e) {
    std::cerr <<"command-line error : " << e.what() << "\n";
    return -1;
  }
  if(input.size() == 0) {
    std::cout << "need input dump\n";
    return -1;
  }
  initCapstone();
  std::ifstream trace_ifs(input, std::ios::binary);
  boost::archive::binary_iarchive rt_(trace_ifs);
  rt_ >> rt;

  double tip_cycles = 0.0;
  for(auto p : rt.tip) {
    tip_cycles += p.second;
  }

  std::cout << "rt.get_records().size() = " <<
    rt.get_records().size() << "\n";
  

  globals::cBB = new basicBlock(rt.get_records().begin()->pc);
  buildCFG(rt.get_records(), counts);

  double ipc = rt.get_records().size() / tip_cycles;
  std::cout << ipc << " ipc\n";

  if(pipe.size() != 0) {
    pt.read(pipe);
  }
  
  
  std::ofstream out("blocks.txt");
  std::vector<std::vector<basicBlock*>> regions;
  std::vector<basicBlock*> r;

  //create region
  for(auto p : basicBlock::bbMap) {
    out << *(p.second) << "\n";
    r.push_back(p.second);
  }
  
  regions.push_back(r);
  regionCFG *cfg = new regionCFG(rt.tip, counts, pt.get_records());
  cfg->buildCFG(regions);
  stopCapstone();

  return 0;
}


