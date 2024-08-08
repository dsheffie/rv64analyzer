#include "interpret.hh"
#include <cassert>         // for assert
#include <cmath>           // for isnan, sqrt
#include <cstdio>          // for printf
#include <cstdlib>         // for exit, abs
#include <iostream>        // for operator<<, basic_ostream<>::__ostream_type
#include <limits>          // for numeric_limits
#include <string>          // for string
#include <type_traits>     // for enable_if, is_integral
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "basicBlock.hh"   // for basicBlock
#include "disassemble.hh"  // for getCondName
#include "helper.hh"       // for extractBit, UNREACHABLE, bswap, setBit
#include "saveState.hh"    // for dumpState
#include "state.hh"        // for state_t, operator<<
#include "riscv.hh"
#define ELIDE_LLVM
#include "globals.hh"      // for cBB, blobName, isMipsEL


static void getNextBlock(uint64_t pc) {
  basicBlock *nBB = globals::cBB->findBlock(pc);
  if(nBB == nullptr ) {
    nBB = new basicBlock(pc, globals::cBB);
  }
  globals::cBB->setReadOnly();
  globals::cBB = nBB;
}

void execRiscv(uint32_t inst, uint64_t pc, uint64_t npc) {
  globals::cBB->addIns(inst, pc);
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


void buildCFG(const std::list<inst_record> &trace) {
  auto nit = trace.begin(); nit++;
  uint64_t cnt = 0;
  for(auto it = trace.begin(), E = trace.end(); nit != E; ++it) {
    uint64_t npc = ~0UL;
    const inst_record & ir = *it;
    if(nit != E) {
      npc = nit->pc;
    }
 
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
      execRiscv(ir.inst, ir.pc, npc);
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


