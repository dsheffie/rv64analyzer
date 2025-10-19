#include <cstdio>
#include <cstdlib>
#include <cassert>
#include "disassemble.hh"
#include "riscvInstruction.hh"
#include "regionCFG.hh"
#include "helper.hh"
#include "globals.hh"

uint64_t ssaInsn::uuid_counter = 0;

std::ostream &operator<<(std::ostream &out, const Insn &ins) {
  out << "0x" << std::hex << ins.addr << std::dec 
      << " : " << getAsmString(ins.inst, ins.addr) 
      << std::endl;
  return out;
}

void ssaInsn::makePrettyName() {
  std::stringstream ss;
  ss << getGPRName(destRegister()) << "_" << uuid;
  prettyName = ss.str(); 
}


void Insn::hookupRegs(MipsRegTable<ssaInsn> &tbl) {
  std::cout << "calling base implementation of hookupRegs\n";
  std::cout << *this;
}

void Insn::dumpSSA(std::ostream &out) const {
  out << getName();
  if(sources.size()) {
    out << " <- ";
    for(auto src : sources) {
      out << src->getName() << " ";
    }
  }
}


class loadInsn : public Insn {
public:
  loadInsn(uint32_t inst, uint64_t addr) : Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {
    cfg->gprDefinitionBlocks[r.l.rd].insert(cBB);
  }
  void recUses(cfgBasicBlock *cBB) override {
    cBB->gprRead[r.l.rs1]=true;
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
      addSrc(tbl.gprTbl[r.l.rs1]);
      tbl.gprTbl[r.l.rd] = this;
  }
};


class atomicInsn : public Insn {
public:
  atomicInsn(uint32_t inst, uint64_t addr) : Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {
    if(r.a.rd != 0) {
      cfg->gprDefinitionBlocks[r.a.rd].insert(cBB);
    }
  }
  void recUses(cfgBasicBlock *cBB) override {
    cBB->gprRead[r.a.rs1]=true;
    cBB->gprRead[r.a.rs2]=true;    
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
      addSrc(tbl.gprTbl[r.a.rs1]);
      addSrc(tbl.gprTbl[r.a.rs2]);      
      if(r.a.rd != 0) {
	tbl.gprTbl[r.a.rd] = this;
      }
  }
};

class fenceInsn : public Insn {
public:
  fenceInsn(uint32_t inst, uint64_t addr) : Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {}
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {}
};

class sretInsn : public Insn {
public:
  sretInsn(uint32_t inst, uint64_t addr) : Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {}
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {}
};

class mretInsn : public Insn {
public:
  mretInsn(uint32_t inst, uint64_t addr) : Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {}
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {}
};


class sfenceInsn : public Insn {
public:
  sfenceInsn(uint32_t inst, uint64_t addr) : Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {}
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {}
};

class wfiInsn : public Insn {
public:
  wfiInsn(uint32_t inst, uint64_t addr) : Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {}
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {}
};

class ebreakInsn : public Insn {
public:
  ebreakInsn(uint32_t inst, uint64_t addr) : Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {}
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {}
};


class csrInsn : public Insn {
protected:
  int rd, rs, csrid;
public:
  csrInsn(uint32_t inst, uint64_t addr) :
    Insn(inst, addr),
    rd((inst>>7) & 31),
    rs((inst >> 15) & 31),
    csrid((inst>>20)) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {
    if(rd != 0) {
      cfg->gprDefinitionBlocks[rd].insert(cBB);
    }
  }
};

class csriInsn : public Insn {
protected:
  int rd, rs, csrid;
public:
  csriInsn(uint32_t inst, uint64_t addr) :
    Insn(inst, addr),
    rd((inst>>7) & 31),
    rs((inst >> 15) & 31),
    csrid((inst>>20)) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {
    if(rd != 0) {
      cfg->gprDefinitionBlocks[rd].insert(cBB);
    }
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
    if(rd != 0) {
      tbl.gprTbl[rd] = this;
    }
  }    
};

class csrrwInsn : public csrInsn {
public:
  csrrwInsn(uint32_t inst, uint64_t addr) : csrInsn(inst, addr) {}
  void recUses(cfgBasicBlock *cBB) override {
    cBB->gprRead[rs]=true;
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
    addSrc(tbl.gprTbl[rs]);
    if(rd != 0) {
      tbl.gprTbl[rd] = this;
    }
  }  
};

class csrrsInsn : public csrInsn {
public:
  csrrsInsn(uint32_t inst, uint64_t addr) : csrInsn(inst, addr) {}
  void recUses(cfgBasicBlock *cBB) override {
    if(rs != 0) {
      cBB->gprRead[rs]=true;
    }
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
    if(rs != 0) {
      addSrc(tbl.gprTbl[rs]);
    }
    if(rd != 0) {
      tbl.gprTbl[rd] = this;
    }
  }  
};

class csrrcInsn : public csrInsn {
public:
  csrrcInsn(uint32_t inst, uint64_t addr) : csrInsn(inst, addr) {}
  void recUses(cfgBasicBlock *cBB) override {
    if(rs != 0) {
      cBB->gprRead[rs]=true;
    }
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
    if(rs != 0) {
      addSrc(tbl.gprTbl[rs]);
    }
    if(rd != 0) {
      tbl.gprTbl[rd] = this;
    }
  }  
};


class storeInsn : public Insn {
public:
  storeInsn(uint32_t inst, uint64_t addr) :
    Insn(inst, addr,insnDefType::no_dest) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {
    cBB->gprRead[r.s.rs1]=true;
    cBB->gprRead[r.s.rs2]=true;
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
    addSrc(tbl.gprTbl[r.s.rs1]);
    addSrc(tbl.gprTbl[r.s.rs2]);
  }

};


/* iType */
class insn_beq : public iBranchTypeInsn {
 public:
 insn_beq(uint32_t inst, uint64_t addr) : iBranchTypeInsn(inst, addr) {}
};

class insn_bne : public iBranchTypeInsn {
 public:
 insn_bne(uint32_t inst, uint64_t addr) : iBranchTypeInsn(inst, addr) {}
};


class insn_blt : public iBranchTypeInsn {
 public:
  insn_blt(uint32_t inst, uint64_t addr) : iBranchTypeInsn(inst, addr) {}
};

class insn_bge : public iBranchTypeInsn {
public:
  insn_bge(uint32_t inst, uint64_t addr) : iBranchTypeInsn(inst, addr) {}
};

class insn_bltu : public iBranchTypeInsn {
 public:
  insn_bltu(uint32_t inst, uint64_t addr) : iBranchTypeInsn(inst, addr) {}
};

class insn_bgeu : public iBranchTypeInsn {
public:
  insn_bgeu(uint32_t inst, uint64_t addr) : iBranchTypeInsn(inst, addr) {}
};

class insn_lui : public Insn {
 public:
 insn_lui(uint32_t inst, uint64_t addr) :
   Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {
    cfg->gprDefinitionBlocks[r.u.rd].insert(cBB);
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
    tbl.gprTbl[r.u.rd] = this;
  }  
};

class insn_nop : public Insn {
public:
  insn_nop(uint32_t inst, uint64_t addr) : Insn(inst, addr) {}
  /* defines nothing */
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}  
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {}
};

class insn_auipc : public Insn {
 public:
  insn_auipc(uint32_t inst, uint64_t addr) : Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {
    cfg->gprDefinitionBlocks[r.u.rd].insert(cBB);
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
    tbl.gprTbl[r.u.rd] = this;
  }
};


class insn_lb : public loadInsn {
public:
  insn_lb(uint32_t inst, uint64_t addr) : loadInsn(inst, addr) {}
};

class insn_lh : public loadInsn {
public:
  insn_lh(uint32_t inst, uint64_t addr) : loadInsn(inst, addr) {}
};

class insn_lw : public loadInsn {
public:
  insn_lw(uint32_t inst, uint64_t addr) : loadInsn(inst, addr) {}
};

class insn_ld : public loadInsn {
public:
  insn_ld(uint32_t inst, uint64_t addr) : loadInsn(inst, addr) {}
};

class insn_lbu : public loadInsn {
 public:
 insn_lbu(uint32_t inst, uint64_t addr) : loadInsn(inst, addr) {}
};

class insn_lhu : public loadInsn {
 public:
 insn_lhu(uint32_t inst, uint64_t addr) : loadInsn(inst, addr) {}
};

class insn_lwu : public loadInsn {
public:
  insn_lwu(uint32_t inst, uint64_t addr) : loadInsn(inst, addr) {}
};



class insn_sb : public storeInsn {
 public:
 insn_sb(uint32_t inst, uint64_t addr) : storeInsn(inst, addr) {}
};

class insn_sh : public storeInsn {
public:
  insn_sh(uint32_t inst, uint64_t addr) : storeInsn(inst, addr) {}
};

class insn_sw : public storeInsn {
public:
  insn_sw(uint32_t inst, uint64_t addr) : storeInsn(inst, addr) {}
};

class insn_sd : public storeInsn {
 public:
  insn_sd(uint32_t inst, uint64_t addr) : storeInsn(inst, addr) {}
};



std::string Insn::getString() const {
  std::stringstream ss;
  ss << *this;
  return ss.str();
}



void Insn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {}

void Insn::recUses(cfgBasicBlock *cBB) {}


void Insn::set(regionCFG *cfg, cfgBasicBlock *cBB) {
  this->cfg = cfg;  myBB = cBB;
}


/* r-type */
void rTypeInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[r.r.rd].insert(cBB);
}

void rTypeInsn::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[r.r.rs1]=true;
  cBB->gprRead[r.r.rs2]=true;
}

void rTypeInsn::hookupRegs(MipsRegTable<ssaInsn> &tbl) {
  addSrc(tbl.gprTbl[r.r.rs1]);
  addSrc(tbl.gprTbl[r.r.rs2]);  
  tbl.gprTbl[r.r.rd] = this;
}

void insn_jalr::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[r.r.rd].insert(cBB);
}

void insn_jalr::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[r.r.rs1]=true;  
}
void insn_jr::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[r.r.rs1]=true;
}

void insn_jalr::hookupRegs(MipsRegTable<ssaInsn> &tbl) {
  addSrc(tbl.gprTbl[r.r.rs1]);
  tbl.gprTbl[r.r.rd] = this;
}

void insn_jr::hookupRegs(MipsRegTable<ssaInsn> &tbl) {
  addSrc(tbl.gprTbl[r.r.rs1]);
}

void iTypeInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[r.i.rd].insert(cBB);
}

void iTypeInsn::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[r.i.rs1]=true;
}

void iTypeInsn::hookupRegs(MipsRegTable<ssaInsn> &tbl) {
  addSrc(tbl.gprTbl[r.i.rs1]);
  tbl.gprTbl[r.i.rd] = this;
}


void iBranchTypeInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  /* don't update anything.. */
}
void iBranchTypeInsn::recUses(cfgBasicBlock *cBB) {
  /* branches read both operands */
  cBB->gprRead[r.b.rs1]=true;
  cBB->gprRead[r.b.rs2]=true;
}

void iBranchTypeInsn::hookupRegs(MipsRegTable<ssaInsn> &tbl) {
  addSrc(tbl.gprTbl[r.b.rs1]);
  addSrc(tbl.gprTbl[r.b.rs2]);  
}

void insn_jal::recDefines(cfgBasicBlock *cBB, regionCFG *cfg)  {
  uint32_t rd = (inst>>7) & 31;
  assert(rd != 0);
  cfg->gprDefinitionBlocks[rd].insert(cBB);
}

void insn_jal::hookupRegs(MipsRegTable<ssaInsn> &tbl) {
  uint32_t rd = (inst>>7) & 31;
  tbl.gprTbl[rd] = this;
}



Insn* getInsn(uint32_t inst, uint64_t addr){
  uint32_t opcode = inst & 127;
  uint32_t rd = (inst>>7) & 31;
  riscv_t m(inst);
  //std::cout << "opcode " << std::hex << opcode << std::dec << "\n";
  switch(opcode)
    {
    case 0x3:  /* loads */
      switch(m.r.sel)
	{
	case 0x0: /* lb */
	  return new insn_lb(inst, addr);
	case 0x1: /* lh */
	  return new insn_lh(inst, addr);
	case 0x2: /* lw */
	  return new insn_lw(inst, addr);
	case 0x3: /* ld */
	  return new insn_ld(inst, addr);
	case 0x4: /* lbu */
	  return new insn_lbu(inst, addr);
	case 0x5:  /* lhu */
	  return new insn_lhu(inst, addr);
	case 0x6: /* lwu */
	  return new insn_lwu(inst, addr);
	}
      break;
    case 0xf:  /* fence - there's a bunch of 'em */
      return new fenceInsn(inst, addr);
    case 0x13: { /* reg + imm insns */
      if ( (((inst>>12) & 7) == 0) and (rd == 0)) {
	return new insn_nop(inst, addr);
      }
      else {
	return new iTypeInsn(inst, addr);
      }
    }
    case 0x17: /* auipc */
      return new insn_auipc(inst, addr);
    case 0x1b:
      return new iTypeInsn(inst, addr);

    case 0x23: {/* stores */
      switch(m.s.sel)
	{
	case 0x0: /* sb */
	  return new insn_sb(inst, addr);
	case 0x1: /* sh */
	  return new insn_sh(inst, addr);
	case 0x2: /* sw */
	  return new insn_sw(inst, addr);
	case 0x3: /* sd */
	  return new insn_sd(inst, addr);
	default:
	  break;
	}
      break;
    }
    case 0x2f:
      return new atomicInsn(inst, addr);
    case 0x37: /* lui */
      return new insn_lui(inst, addr);
    case 0x67: {/* jalr */
      return (rd==0) ? dynamic_cast<Insn*>(new insn_jr(inst, addr)) :
	dynamic_cast<Insn*>(new insn_jalr(inst,addr));
    }
    case 0x6f: {/* jal */
      return (rd==0) ? dynamic_cast<Insn*>(new insn_j(inst, addr)) :
	dynamic_cast<Insn*>(new insn_jal(inst,addr));
    }
    case 0x33:
    case 0x3b: /* reg + reg insns */
      return new rTypeInsn(inst, addr);
    case 0x63: /* branches */
      switch(m.b.sel)
	{
	case 0: /* beq */
	  return new insn_beq(inst, addr);
	case 1: /* bne */
	  return new insn_bne(inst, addr);
	case 4: /* blt */
	  return new insn_blt(inst, addr);	  
	case 5: /* bge */
	  return new insn_bge(inst, addr);	  	  
	case 6: /* bltu */
	  return new insn_bltu(inst, addr);	  
	case 7: /* bgeu */
	  return new insn_bgeu(inst, addr);	  	  
	default:
	  break;
	}
      break;
    case 0x73: {
      uint32_t csr_id = (inst>>20);
      bool is_ecall = ((inst >> 7) == 0);
      bool is_ebreak = ((inst>>7) == 0x2000);
      bool bits19to7z = (((inst >> 7) & 8191) == 0);
      uint64_t upper7 = (inst>>25);
      if(is_ecall) {
	die();
      }
      else if(upper7 == 9 && ((inst & (16384-1)) == 0x73 )) {
	return new sfenceInsn(inst, addr);
      }
      else if(bits19to7z and (csr_id == 0x105)) {
	/* wfi */
	return new wfiInsn(inst, addr);
      }
      else if(bits19to7z and (csr_id == 0x002)) {  /* uret */
	assert(false);
      }
      else if(bits19to7z and (csr_id == 0x102)) {  /* sret */
	return new sretInsn(inst, addr);
      }
      else if(bits19to7z and (csr_id == 0x202)) {  /* hret */
	die();
      }            
      else if(bits19to7z and (csr_id == 0x302)) {  /* mret */
	return new mretInsn(inst, addr);
      }
      else if(is_ebreak) {
	return new ebreakInsn(inst, addr);
      }
      else {
	switch((inst>>12) & 7)
	  {	    
	  case 1: { /* CSRRW */
	    return new csrrwInsn(inst, addr);
	  }
	  case 2: {/* CSRRS */
	    return new csrrsInsn(inst, addr);
	  }
	  case 3: {/* CSRRC */
	    return new csrrcInsn(inst, addr);
	  }
	  case 5: {/* CSRRWI */
	    return new csriInsn(inst, addr);
	  }
	  case 6:{ /* CSRRSI */
	    return new csriInsn(inst, addr);
	  }
	  case 7: {/* CSRRCI */
	    return new csriInsn(inst, addr);	    
	  }
	  default:
	    break;
	  }
	std::cout << "very confused at pc " << std::hex << addr << std::dec << "\n";
	die();	
	return nullptr;	
      }
      std::cout << "very confused at pc " << std::hex << addr << ", raw = " << inst << std::dec << "\n";
      disassemble(std::cout, inst, addr);
      std::cout << "\n";
      die();
      return nullptr;
    }
    default:
      std::cout << "what is opcode " << std::hex << opcode << std::dec << "\n";
      break;
    }
  
  return new Insn(inst, addr);
}




