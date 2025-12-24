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

class memInsn : public Insn {
public:
  memInsn(uint32_t inst, uint64_t addr, insnDefType insnType = insnDefType::unknown) : Insn(inst, addr, insnType) {}
  virtual int64_t computeDisp() const {
    return 0;
  }  
};


class loadInsn : public memInsn {
protected:
  enum class subType {unknown,lb,lbu,lh,lhu,lw,lwu,ld};
  subType st;
public:
  loadInsn(uint32_t inst, uint64_t addr, subType st = subType::unknown) : memInsn(inst, addr), st(st) {}
  
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {
    if(r.l.rd != 0) {
      cfg->gprDefinitionBlocks[r.l.rd].insert(cBB);
    }
  }
  void recUses(cfgBasicBlock *cBB) override {
    cBB->gprRead[r.l.rs1]=true;
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
      addSrc(tbl.gprTbl[r.l.rs1]);
      tbl.gprTbl[r.l.rd] = this;
  }
  void dumpSSA(std::ostream &out) const override;
  bool isLoad() const override {
    return true;
  }
  int64_t computeDisp() const override {
    int32_t disp = r.l.imm11_0;
    if((inst>>31)&1) {
      disp |= 0xfffff000;
    }
    int64_t disp64 = disp;
    return ((disp64 << 32) >> 32);
  }
};


class atomicInsn : public memInsn {
public:
  atomicInsn(uint32_t inst, uint64_t addr) : memInsn(inst, addr) {}
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
  bool isLoad() const override {
    return true;
  }
  bool isStore() const override {
    return true;
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


class storeInsn : public memInsn {
protected:
  enum class subType {unknown,sb,sh,sw,sd};
  subType st;
public:
  storeInsn(uint32_t inst, uint64_t addr, subType st = subType::unknown) :
    memInsn(inst, addr,insnDefType::no_dest) {}
  bool isStore() const override {
    return true;
  }
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {
    cBB->gprRead[r.s.rs1]=true;
    cBB->gprRead[r.s.rs2]=true;
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
    addSrc(tbl.gprTbl[r.s.rs1]);
    addSrc(tbl.gprTbl[r.s.rs2]);
  }
  int64_t computeDisp() const override {
    int32_t disp = r.s.imm4_0 | (r.s.imm11_5 << 5);    
    disp |= ((inst>>31)&1) ? 0xfffff000 : 0x0;
    int64_t disp64 = disp;
    return ((disp64 << 32) >> 32);
  }  
  void dumpSSA(std::ostream &out) const override;
};


/* iType */
class insn_beq : public iBranchTypeInsn {
 public:
  insn_beq(uint32_t inst, uint64_t addr) : iBranchTypeInsn(inst, addr, subType::beq) {}
};

class insn_bne : public iBranchTypeInsn {
 public:
  insn_bne(uint32_t inst, uint64_t addr) : iBranchTypeInsn(inst, addr, subType::bne) {}
};

class insn_blt : public iBranchTypeInsn {
 public:
  insn_blt(uint32_t inst, uint64_t addr) : iBranchTypeInsn(inst, addr, subType::blt) {}
};

class insn_bge : public iBranchTypeInsn {
public:
  insn_bge(uint32_t inst, uint64_t addr) : iBranchTypeInsn(inst, addr, subType::bge) {}
};

class insn_bltu : public iBranchTypeInsn {
 public:
  insn_bltu(uint32_t inst, uint64_t addr) : iBranchTypeInsn(inst, addr, subType::bltu) {}
};

class insn_bgeu : public iBranchTypeInsn {
public:
  insn_bgeu(uint32_t inst, uint64_t addr) : iBranchTypeInsn(inst, addr, subType::bgeu) {}
};

class insn_lui : public Insn {
 public:
 insn_lui(uint32_t inst, uint64_t addr) :
   Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {
    if(r.u.rd != 0) {
      cfg->gprDefinitionBlocks[r.u.rd].insert(cBB);
    }
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
    tbl.gprTbl[r.u.rd] = this;
  }
  int64_t getImm() const {
    int32_t imm32 = inst & 0xfffff000;
    int64_t y = imm32;
    y = (y << 32) >> 32;
    return y;
  }
  void dumpSSA(std::ostream &out) const override {
    out <<  getName() << " <- lui " << std::hex << getImm() << std::dec << " ";
  }    
};

class insn_nop : public Insn {
public:
  insn_nop(uint32_t inst, uint64_t addr) : Insn(inst, addr) {}
  /* defines nothing */
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}  
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {}
  void dumpSSA(std::ostream &out) const override {
    out << "nop ";
  }
};

class insn_auipc : public Insn {
 public:
  insn_auipc(uint32_t inst, uint64_t addr) : Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {
    if(r.u.rd != 0) {
      cfg->gprDefinitionBlocks[r.u.rd].insert(cBB);
    }
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {
    tbl.gprTbl[r.u.rd] = this;
  }
  int64_t getImm() const {
    int64_t imm = inst & (~4095);
    imm = (imm << 32) >> 32;
    int64_t y = addr + imm;
    return y;
  }
  void dumpSSA(std::ostream &out) const override {
    out <<  getName() << " <- auipc " << std::hex << getImm() << std::dec << " ";
  }  
};


class insn_lb : public loadInsn {
public:
  insn_lb(uint32_t inst, uint64_t addr) : loadInsn(inst, addr, subType::lb) {}
};

class insn_lh : public loadInsn {
public:
  insn_lh(uint32_t inst, uint64_t addr) : loadInsn(inst, addr, subType::lh) {}
};

class insn_lw : public loadInsn {
public:
  insn_lw(uint32_t inst, uint64_t addr) : loadInsn(inst, addr, subType::lw) {}
};

class insn_ld : public loadInsn {
public:
  insn_ld(uint32_t inst, uint64_t addr) : loadInsn(inst, addr, subType::ld) {}
};

class insn_lbu : public loadInsn {
 public:
  insn_lbu(uint32_t inst, uint64_t addr) : loadInsn(inst, addr, subType::lbu) {}
};

class insn_lhu : public loadInsn {
 public:
  insn_lhu(uint32_t inst, uint64_t addr) : loadInsn(inst, addr, subType::lhu) {}
};

class insn_lwu : public loadInsn {
public:
  insn_lwu(uint32_t inst, uint64_t addr) : loadInsn(inst, addr, subType::lwu) {}
};

class insn_sb : public storeInsn {
 public:
  insn_sb(uint32_t inst, uint64_t addr) : storeInsn(inst, addr, subType::sb) {}
};

class insn_sh : public storeInsn {
public:
  insn_sh(uint32_t inst, uint64_t addr) : storeInsn(inst, addr, subType::sh) {}
};

class insn_sw : public storeInsn {
public:
  insn_sw(uint32_t inst, uint64_t addr) : storeInsn(inst, addr, subType::sw) {}
};

class insn_sd : public storeInsn {
 public:
  insn_sd(uint32_t inst, uint64_t addr) : storeInsn(inst, addr, subType::sd) {}
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
  if(r.r.rd != 0) {
    cfg->gprDefinitionBlocks[r.r.rd].insert(cBB);
  }
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

void rTypeInsn::dumpSSA(std::ostream &out) const {
  out << getName();
  
  out << " <- ";
  switch(st)
    {
    case subType::add:
      out << "add ";
      break;
    case subType::mul:
      out << "mul ";
      break;
    case subType::sub:
      out << "sub ";
      break;
    case subType::div:
      out << "div ";
      break;
    case subType::min:
      out << "min ";
      break;      
    case subType::sh2add:
      out << "sh2add ";
      break;
    case subType::xnor:
      out << "xnor ";
      break;      
    case subType::xor_:
      out << "xor ";
      break;
    case subType::sll:
      out << "sll ";
      break;
    case subType::mulh:
      out << "mulh ";
      break;
    case subType::rol:
      out << "rol ";
      break;      
    case subType::slt:
      out << "slt ";
      break;      
    case subType::sh1add:
      out << "sh1add ";
      break;      
    case subType::sltu:
      out << "sltu ";
      break;      
    case subType::mulhu:
      out << "mulhu ";
      break;
    case subType::srl:
      out << "srl ";
      break;
    case subType::divu:
      out << "divu ";
      break;
    case subType::minu:
      out << "minu ";
      break;
    case subType::czeqz:
      out << "czeqz ";
      break;
    case subType::sra:
      out << "sra ";
      break;
    case subType::ror:
      out << "ror ";
      break;
    case subType::or_:
      out << "or ";
      break;
    case subType::rem:
      out << "rem ";
      break;
    case subType::max:
      out << "max ";
      break;
    case subType::sh3add:
      out << "sh3add ";
      break;
    case subType::orn:
      out << "orn ";
      break;
    case subType::and_:
      out << "and ";
      break;
    case subType::remu:
      out << "remu ";
      break;
    case subType::maxu:
      out << "maxu ";
      break;
    case subType::cznez:
      out << "cznez ";
      break;
    case subType::andn:
      out << "andn ";
      break;
    case subType::addw:
      out << "andw ";
      break;
    case subType::subw:
      out << "subw ";
      break;
    case subType::rolw:
      out << "rolw ";
      break;
    case subType::sh1adduw:
      out << "sh1adduw ";
      break;
    case subType::sh2adduw:
      out << "sh2adduw ";
      break;
    case subType::zexth:
      out << "zexth ";
      break;
    case subType::rorw:
      out << "rorw ";
      break;
    case subType::sh3adduw:
      out << "sh3adduw ";
      break;
    case subType::mulw:
      out << "mulw ";
      break;
    case subType::adduw:
      out << "adduw ";
      break;
    case subType::sllw:
      out << "sllw ";
      break;
    case subType::divw:
      out << "divw ";
      break;
    case subType::srlw:
      out << "srlw ";
      break;
    case subType::divuw:
      out << "divuw ";
      break;
    case subType::sraw:
      out << "sraw ";
      break;
    case subType::remw:
      out << "andn ";
      break;
    case subType::remuw:
      out << "remuw ";
      break;
    default:
      out << "rtypehuh ";
    }
  for(auto src : sources) {
    out << src->getName() << " ";
  }
}


void insn_jalr::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  if(r.r.rd != 0) {
    cfg->gprDefinitionBlocks[r.r.rd].insert(cBB);
  }
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
  if(r.i.rd != 0) {
    cfg->gprDefinitionBlocks[r.i.rd].insert(cBB);
  }
}

void iTypeInsn::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[r.i.rs1]=true;
}

void iTypeInsn::hookupRegs(MipsRegTable<ssaInsn> &tbl) {
  addSrc(tbl.gprTbl[r.i.rs1]);
  tbl.gprTbl[r.i.rd] = this;
}


void iTypeInsn::dumpSSA(std::ostream &out) const {
  out << getName() << " <- ";
  switch(st)
    {
    case subType::addi:
      out << "addi ";
      break;
    case subType::mv:
      out << "mv ";
      break;
    case subType::clz:
      out << "clz ";
      break;
    case subType::ctz:
      out << "ctz ";
      break;
    case subType::cpop:
      out << "cpop ";
      break;
    case subType::sextb:
      out << "sextb ";
      break;
    case subType::sexth:
      out << "sexth ";
      break;
    case subType::slli:
      out << "slli ";
      break;
    case subType::slti:
      out << "slti ";
      break;
    case subType::sltiu:
      out << "sltiu ";
      break;
    case subType::xori:
      out << "xori ";
      break;
    case subType::srli:
      out << "srli ";
      break;
    case subType::orcb:
      out << "orcb ";
      break;
    case subType::srai:
      out << "srai ";
      break;
    case subType::rori:
      out << "rori ";
      break;
    case subType::rev8:
      out << "rev8 ";
      break;
    case subType::ori:
      out << "ori ";
      break;
    case subType::andi:
      out << "andi ";
      break;
    case subType::addiw:
      out << "addiw ";
      break;
    case subType::slliw:
      out << "slliw ";
      break;
    case subType::slliuw:
      out << "slliuw ";
      break;
    case subType::clzw:
      out << "clzw ";
      break;
    case subType::ctzw:
      out << "ctzw ";
      break;
    case subType::cpopw:
      out << "cpopw ";
      break;
    case subType::srliw:
      out << "srliw ";
      break;
     case subType::sraiw:
      out << "sraiw ";
      break;     
     case subType::roriw:
       out << "roriw ";
      break;     

    default:
      out << "itypehuh with sel = " << r.i.sel << " ";
    }
  for(auto src : sources) {
    out << src->getName() << " ";
  }
  if(st != subType::mv) {
    out << std::hex << getImm() << std::dec << " ";
  }
}

class insn_addi : public iTypeInsn  {
public:
  insn_addi(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::addi) {}
};

class insn_mv : public iTypeInsn  {
public:
  insn_mv(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::mv) {}
};

class insn_slli : public iTypeInsn  {
public:
  insn_slli(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::slli) {}
};

class insn_clz : public iTypeInsn  {
public:
  insn_clz(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::clz) {}
};

class insn_ctz : public iTypeInsn  {
public:
  insn_ctz(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::ctz) {}
};

class insn_cpop : public iTypeInsn  {
public:
  insn_cpop(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::cpop) {}
};

class insn_sextb : public iTypeInsn  {
public:
  insn_sextb(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::sextb) {}
};

class insn_sexth : public iTypeInsn  {
public:
  insn_sexth(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::sexth) {}
};

class insn_slti : public iTypeInsn  {
public:
  insn_slti(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::slti) {}
};

class insn_sltiu : public iTypeInsn  {
public:
  insn_sltiu(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::sltiu) {}
};

class insn_xori : public iTypeInsn  {
public:
  insn_xori(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::xori) {}
};

class insn_srli : public iTypeInsn  {
public:
  insn_srli(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::srli) {}
};

class insn_orcb : public iTypeInsn  {
public:
  insn_orcb(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::orcb) {}
};

class insn_srai : public iTypeInsn  {
public:
  insn_srai(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::srai) {}
};

class insn_rori : public iTypeInsn  {
public:
  insn_rori(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::rori) {}
};

class insn_rev8 : public iTypeInsn  {
public:
  insn_rev8(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::rev8) {}
};

class insn_ori : public iTypeInsn  {
public:
  insn_ori(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::ori) {}
};

class insn_andi : public iTypeInsn  {
public:
  insn_andi(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::andi) {}
};

class insn_addiw : public iTypeInsn  {
public:
  insn_addiw(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::addiw) {}
};

class insn_slliw : public iTypeInsn  {
public:
  insn_slliw(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::slliw) {}
};

class insn_slliuw : public iTypeInsn  {
public:
  insn_slliuw(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::slliuw) {}
};

class insn_clzw : public iTypeInsn  {
public:
  insn_clzw(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::clzw) {}
};

class insn_ctzw : public iTypeInsn  {
public:
  insn_ctzw(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::ctzw) {}
};

class insn_cpopw : public iTypeInsn  {
public:
  insn_cpopw(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::cpopw) {}
};

class insn_srliw : public iTypeInsn  {
public:
  insn_srliw(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::srliw) {}
};

class insn_sraiw : public iTypeInsn  {
public:
  insn_sraiw(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::sraiw) {}
};

class insn_roriw : public iTypeInsn  {
public:
  insn_roriw(uint32_t inst, uint64_t addr) :
    iTypeInsn(inst, addr, insnDefType::gpr, subType::roriw) {}
};


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
  if(rd != 0) {
    cfg->gprDefinitionBlocks[rd].insert(cBB);
  }
}

void insn_jal::hookupRegs(MipsRegTable<ssaInsn> &tbl) {
  uint32_t rd = (inst>>7) & 31;
  tbl.gprTbl[rd] = this;
}



class insn_add : public rTypeInsn  {
public:
  insn_add(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::add) {}
};

class insn_sub : public rTypeInsn  {
public:
  insn_sub(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::sub) {}
};

class insn_mul : public rTypeInsn  {
public:
  insn_mul(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::mul) {}
};

class insn_div : public rTypeInsn  {
public:
  insn_div(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::div) {}
};

class insn_min : public rTypeInsn  {
public:
  insn_min(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::min) {}
};

class insn_sh2add : public rTypeInsn  {
public:
  insn_sh2add(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::sh2add) {}
};

class insn_xnor : public rTypeInsn  {
public:
  insn_xnor(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::xnor) {}
};

class insn_xor : public rTypeInsn  {
public:
  insn_xor(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::xor_) {}
};

class insn_sll : public rTypeInsn  {
public:
  insn_sll(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::sll) {}
};

class insn_mulh : public rTypeInsn  {
public:
  insn_mulh(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::mulh) {}
};

class insn_rol : public rTypeInsn  {
public:
  insn_rol(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::rol) {}
};

class insn_sh1add : public rTypeInsn  {
public:
  insn_sh1add(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::sh1add) {}
};

class insn_slt : public rTypeInsn  {
public:
  insn_slt(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::slt) {}
};

class insn_sltu : public rTypeInsn  {
public:
  insn_sltu(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::sltu) {}
};

class insn_mulhu : public rTypeInsn  {
public:
  insn_mulhu(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::mulhu) {}
};

class insn_and : public rTypeInsn  {
public:
  insn_and(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::and_) {}
};

class insn_remu : public rTypeInsn  {
public:
  insn_remu(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::remu) {}
};

class insn_maxu : public rTypeInsn  {
public:
  insn_maxu(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::maxu) {}
};

class insn_cznez : public rTypeInsn  {
public:
  insn_cznez(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::cznez) {}
};

class insn_andn : public rTypeInsn  {
public:
  insn_andn(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::andn) {}
};

//srl, divu, minu, czeqz, sra, ror,

class insn_srl : public rTypeInsn  {
public:
  insn_srl(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::srl) {}
};

class insn_divu : public rTypeInsn  {
public:
  insn_divu(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::divu) {}
};

class insn_minu : public rTypeInsn  {
public:
  insn_minu(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::minu) {}
};

class insn_czeqz : public rTypeInsn  {
public:
  insn_czeqz(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::czeqz) {}
};

class insn_sra : public rTypeInsn  {
public:
  insn_sra(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::sra) {}
};

class insn_ror : public rTypeInsn  {
public:
  insn_ror(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::ror) {}
};

//or_, rem, max, sh3add, orn};

class insn_or : public rTypeInsn  {
public:
  insn_or(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::or_) {}
};

class insn_rem : public rTypeInsn  {
public:
  insn_rem(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::rem) {}
};

class insn_max : public rTypeInsn  {
public:
  insn_max(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::max) {}
};

class insn_sh3add : public rTypeInsn  {
public:
  insn_sh3add(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::sh3add) {}
};

class insn_orn : public rTypeInsn  {
public:
  insn_orn(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::orn) {}
};

class insn_addw : public rTypeInsn  {
public:
  insn_addw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::addw) {}
};

class insn_subw : public rTypeInsn  {
public:
  insn_subw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::subw) {}
};

class insn_rolw : public rTypeInsn  {
public:
  insn_rolw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::rolw) {}
};

class insn_sh1adduw : public rTypeInsn  {
public:
  insn_sh1adduw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::sh1adduw) {}
};

class insn_sh2adduw : public rTypeInsn  {
public:
  insn_sh2adduw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::sh2adduw) {}
};

class insn_zexth : public rTypeInsn  {
public:
  insn_zexth(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::zexth) {}
};

class insn_rorw : public rTypeInsn  {
public:
  insn_rorw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::rorw) {}
};

class insn_sh3adduw : public rTypeInsn  {
public:
  insn_sh3adduw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::sh3adduw) {}
};

class insn_mulw : public rTypeInsn  {
public:
  insn_mulw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::mulw) {}
};

class insn_adduw : public rTypeInsn  {
public:
  insn_adduw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::adduw) {}
};

class insn_sllw : public rTypeInsn  {
public:
  insn_sllw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::sllw) {}
};

class insn_divw : public rTypeInsn  {
public:
  insn_divw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::divw) {}
};

class insn_srlw : public rTypeInsn  {
public:
  insn_srlw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::srlw) {}
};

class insn_divuw : public rTypeInsn  {
public:
  insn_divuw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::divuw) {}
};

class insn_sraw : public rTypeInsn  {
public:
  insn_sraw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::sraw) {}
};

class insn_remw : public rTypeInsn  {
public:
  insn_remw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::remw) {}
};

class insn_remuw : public rTypeInsn  {
public:
  insn_remuw(uint32_t inst, uint64_t addr) :
    rTypeInsn(inst, addr, insnDefType::gpr, subType::remuw) {}
};


inline static Insn* decodeRtype(uint32_t inst, uint64_t addr){
  uint32_t opcode = inst & 127;
  riscv_t m(inst);
  
  if(m.r.rd == 0) {
    return new insn_nop(inst, addr);
  }
  
  if(opcode == 0x33) {
    switch(m.r.sel)
      {
      case 0x0:
	if(m.r.special == 0x0) {
	  return new insn_add(inst, addr);
	}
	else if(m.r.special == 0x1) {
	  return new insn_mul(inst, addr);
	}
	else if(m.r.special == 0x20) {
	  return new insn_sub(inst, addr);
	}
	break;
      case 0x1:
	if(m.r.special == 0x0) {
	  return new insn_sll(inst, addr);
	}
	else if(m.r.special == 0x1) {
	  return new insn_mulh(inst, addr);
	}
	else if(m.r.special == 0x30) {
	  return new insn_rol(inst, addr);
	}
	break;
      case 0x2:
	if(m.r.special == 0x0) {
	  return new insn_slt(inst, addr);
	}
	else if(m.r.special == 0x10) {
	  return new insn_sh1add(inst, addr);
	}
	break;
      case 0x3:
	if(m.r.special == 0x0) {
	  return new insn_sltu(inst, addr);
	}
	else if(m.r.special == 0x1) {
	  return new insn_mulhu(inst, addr);
	}
	break;
      case 0x4:
	if(m.r.special == 0x0) {
	  return new insn_xor(inst, addr);
	}
	else if(m.r.special == 0x1) {
	  return new insn_div(inst, addr);
	}
	else if(m.r.special == 0x5) {
	  return new insn_min(inst, addr);
	}
	else if(m.r.special == 0x10) {
	  return new insn_sh2add(inst, addr);
	}
	else if(m.r.special == 0x20) {
	  return new insn_xnor(inst, addr);
	}
	break;
      case 0x5:
	if(m.r.special == 0x0) {
	  return new insn_srl(inst, addr);
	}
	else if(m.r.special == 0x1) {
	  return new insn_divu(inst, addr);
	}
	else if(m.r.special == 0x5) {
	  return new insn_minu(inst, addr);
	}
	else if(m.r.special == 0x7) {
	  return new insn_czeqz(inst, addr);
	}
	else if(m.r.special == 0x20) {
	  return new insn_sra(inst, addr);
	}
	else if(m.r.special == 0x30) {
	  return new insn_ror(inst, addr);
	}	
	break;
      case 0x6:
	if(m.r.special == 0x0) {
	  return new insn_or(inst, addr);
	}
	else if(m.r.special == 0x1) {
	  return new insn_rem(inst, addr);
	}
	else if(m.r.special == 0x5) {
	  return new insn_max(inst, addr);
	}
	else if(m.r.special == 0x10) {
	  return new insn_sh3add(inst, addr);
	}
	else if(m.r.special == 0x20) {
	  return new insn_orn(inst, addr);
	}	
	break;
      case 0x7:
	if(m.r.special == 0x0) {
	  return new insn_and(inst, addr);
	}
	else if(m.r.special == 0x1) {
	  return new insn_remu(inst, addr);
	}
	else if(m.r.special == 0x5) {
	  return new insn_maxu(inst, addr);
	}
	else if(m.r.special == 0x7) {
	  return new insn_cznez(inst, addr);
	}
	else if(m.r.special == 0x20) {
	  return new insn_andn(inst, addr);
	}
	break;
      default:
	break;
      }
  }
  else if(opcode == 0x3b) {

  }
  return new rTypeInsn(inst, addr);  
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
	if(m.i.sel == 0) {
	  return ((inst >> 20) == 0) ? reinterpret_cast<Insn*>(new insn_mv(inst, addr)) :
	    reinterpret_cast<Insn*>(new insn_addi(inst, addr));
	}
	else if(m.i.sel == 1) {
	  switch((inst>>20) & 4095)
	    {
	    case 0x600:
	      return new insn_clz(inst,addr);
	    case 0x601:
	      return new insn_ctz(inst,addr);	      
	    case 0x602:
	      return new insn_cpop(inst,addr);
	    case 0x604:
	      return new insn_sextb(inst,addr);
	    case 0x605:
	      return new insn_sexth(inst,addr);
	    default:
	      break;
	    }
	  return new insn_slli(inst, addr);	  
	}
	else if(m.i.sel == 2) {
	  return new insn_slti(inst, addr);
	}
	else if(m.i.sel == 3) {
	  return new insn_sltiu(inst, addr);
	}	
	else if(m.i.sel == 4) {
	  return new insn_xori(inst, addr);
	}
	else if(m.i.sel == 5) {
	  switch((inst>>26) & 63)
	    {
	    case 0x0:
	      return new insn_srli(inst, addr);
	    case 0xa:
	      return new insn_orcb(inst, addr);
	    case 0x10:
	      return new insn_srai(inst, addr);
	    case 0x18:
	      return new insn_rori(inst, addr);
	    case 0x1a:
	      return new insn_rev8(inst, addr);
	    default:
	      break;
	    }
	  return new iTypeInsn(inst, addr);
	}
	else if(m.i.sel == 6) {
	  return new insn_ori(inst, addr);
	}
	else {
	  return new insn_andi(inst, addr);
	}
      }
    }
    case 0x17: /* auipc */
      return new insn_auipc(inst, addr);
    case 0x1b: {
      if ( (((inst>>12) & 7) == 0) and (rd == 0)) {
	return new insn_nop(inst, addr);
      }
      else if(m.i.sel == 0) {
	return new insn_addiw(inst, addr);
      }
      else if(m.i.sel == 1) {
	uint32_t sel = ((inst>>26)&63);
	if(sel == 0) {
	  return new insn_slliw(inst, addr);
	}
	else if(sel == 2) {
	  return new insn_slliuw(inst, addr);
	}
	else if(sel == 0x18) {
	  if( ((inst>>20)&31) == 0) {
	    return new insn_clzw(inst, addr);
	  }
	  else if(((inst>>20)&31) == 1) {
	    return new insn_ctzw(inst, addr);
	  }
	  else if(((inst>>20)&31) == 1) {
	    return new insn_cpopw(inst, addr);
	  }	  
	}
      }
      else if(m.i.sel == 5) {
	uint32_t sel =  (inst >> 25) & 127;
	if(sel == 0x0) {
	  return new insn_srliw(inst, addr);
	}
	else if(sel == 0x20) {
	  return new insn_sraiw(inst, addr);
	}
	else if(sel == 0x30) {
	  return new insn_roriw(inst, addr);
	}
      }
      return new iTypeInsn(inst, addr);
    }
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
      return decodeRtype(inst, addr);
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



void loadInsn::dumpSSA(std::ostream &out) const {
  out << getName();  
  out << " <- ";
  switch(st)
    {
    case subType::lb:
      out << "lb ";
      break;
    case subType::lh:
      out << "lh ";
      break;
    case subType::lw:
      out << "lw ";
      break;
    case subType::ld:
      out << "ld ";
      break;                              
    case subType::lwu:
      out << "lwu ";
      break;
    case subType::lbu:
      out << "lbu ";
      break;
    case subType::lhu:
      out << "lhu ";
      break;
    default:
      break;
    }
  for(auto src : sources) {
    out << src->getName() << " ";
  }
  out << computeDisp() << " ";
}


void storeInsn::dumpSSA(std::ostream &out) const {
  switch(st)
    {
    case subType::sb:
      out << "sb ";
      break;
    case subType::sh:
      out << "sh ";
      break;
    case subType::sw:
      out << "sw ";
      break;
    case subType::sd:
      out << "sd ";
      break;                              
    default:
      break;
    }
  for(auto src : sources) {
    out << src->getName() << " ";
  }
  out << computeDisp() << " ";  
}


void iBranchTypeInsn::dumpSSA(std::ostream &out) const {
  switch(st)
    {
    case subType::beq:
      out << "beq ";
      break;
    case subType::bne:
      out << "bne ";
      break;
    case subType::blt:
      out << "blt ";
      break;
    case subType::bge:
      out << "bge ";
      break;
    case subType::bltu:
      out << "bltu ";
      break;
    case subType::bgeu:
      out << "bgeu ";
      break;
    default:
      break;
    }
  for(auto src : sources) {
    out << src->getName() << " ";
  }
  out << std::hex << tAddr << std::dec << " ";
}
