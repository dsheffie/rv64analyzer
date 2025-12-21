#ifndef __SIM_MIPS_INSN__
#define __SIM_MIPS_INSN__

#include <string>
#include <cstdint>
#include "ssaInsn.hh"
#include "riscv.hh"

class cfgBasicBlock;
class regionCFG;

class Insn;
enum regEnum {uninit=0,constant,variant};
enum opPrecType {integerprec=0,singleprec,doubleprec,fpspecialprec,unknownprec,dummyprec};

Insn* getInsn(uint32_t inst, uint64_t addr);

class Insn : public ssaInsn {
protected:
  friend std::ostream &operator<<(std::ostream &out, const Insn &ins);
  uint32_t inst;
  uint64_t addr;
  riscv_t r;
  regionCFG *cfg = nullptr;
  cfgBasicBlock *myBB = nullptr;
  
public:
  void set(regionCFG *cfg, cfgBasicBlock *cBB);
  size_t getInsnId();
  std::string getString() const;
  
  virtual void recDefines(cfgBasicBlock *cBB, regionCFG *cfg);
  virtual void recUses(cfgBasicBlock *cBB);
  
  virtual opPrecType getPrecType() const {
    return integerprec;
  }
  int32_t destRegister() const override {
    uint32_t rd = (inst>>7) & 31;
    return (rd == 0) ? -1 : static_cast<int32_t>(rd);
  }  
  uint64_t getAddr() const {
    return addr;
  }
  Insn(uint32_t inst, uint64_t addr, insnDefType insnType = insnDefType::unknown) :
    ssaInsn(insnType), inst(inst), addr(addr), r(inst) {
  }
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override;
  void dumpSSA(std::ostream &out) const override;
};


class iTypeInsn : public Insn  {
protected:
  enum class subType {unknown, addi, mv, clz, ctz, cpop, sextb, sexth, slli,
		      slti, sltiu, xori, srli, orcb, srai, rori, rev8,
		      ori, andi};
  subType st;
public:
  iTypeInsn(uint32_t inst, uint64_t addr, insnDefType insnType = insnDefType::gpr, subType st = subType::unknown) : 
    Insn(inst, addr, insnType), st(st) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;  
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override;
  void dumpSSA(std::ostream &out) const override;
  int64_t getImm() const {
    int32_t simm32 = (inst >> 20);
    simm32 |= ((inst>>31)&1) ? 0xfffff000 : 0x0;
    int64_t simm64 = simm32;
    return (simm64 <<32) >> 32;
  }
};

class iBranchTypeInsn : public Insn {
protected:
  enum class subType {unknown, beq, bne, blt, bge, bltu, bgeu};
  subType st;  
  int64_t tAddr=0,ntAddr=0;
public:
  iBranchTypeInsn(uint32_t inst, uint64_t addr, subType st = subType::unknown ) : 
    Insn(inst, addr, insnDefType::no_dest), st(st) {
    int32_t disp =
      (r.b.imm4_1 << 1)  |
      (r.b.imm10_5 << 5) |	
      (r.b.imm11 << 11)  |
      (r.b.imm12 << 12);
    disp |= r.b.imm12 ? 0xffffe000 : 0x0;
    tAddr = disp + addr;
    ntAddr = addr + 4;
  }
  uint64_t getTakenAddr() const { return tAddr; }
  uint64_t getNotTakenAddr() const { return ntAddr; }
  bool isControlFlow() const override { return true; }  
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override ;
  void recUses(cfgBasicBlock *cBB) override;
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override;
  void dumpSSA(std::ostream &out) const override;  
};


class rTypeInsn : public Insn {
protected:
  enum class subType {unknown,add,mul,sub,div,min,sh2add,xnor,xor_,
		      sll, mulh, rol, slt, sh1add, sltu, mulhu,
		      srl, divu, minu, czeqz, sra, ror,
		      or_, rem, max, sh3add, orn,
		      and_, remu, maxu, cznez, andn};
  subType st;
public:
  rTypeInsn(uint32_t inst, uint64_t addr, insnDefType insnType = insnDefType::gpr, subType st = subType::unknown) :
    Insn(inst, addr, insnType), st(st) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override;
  void dumpSSA(std::ostream &out) const override;  
};

class insn_j : public Insn {
public:
  insn_j(uint32_t inst, uint64_t addr) :
    Insn(inst, addr, insnDefType::no_dest) {}
  int32_t getJumpAddr() const {
    int32_t jaddr =
      (r.j.imm10_1 << 1)   |
      (r.j.imm11 << 11)    |
      (r.j.imm19_12 << 12) |
      (r.j.imm20 << 20);
    jaddr |= ((inst>>31)&1) ? 0xffe00000 : 0x0;
    jaddr += addr;
    return jaddr;
  }
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {}
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override {}
};

class insn_jal : public Insn {
public:
  insn_jal(uint32_t inst, uint64_t addr) :
    Insn(inst, addr, insnDefType::gpr) {}
  int32_t getJumpAddr() const {
    int32_t jaddr =
      (r.j.imm10_1 << 1)   |
      (r.j.imm11 << 11)    |
      (r.j.imm19_12 << 12) |
      (r.j.imm20 << 20);
    jaddr |= ((inst>>31)&1) ? 0xffe00000 : 0x0;
    jaddr += addr;
    return jaddr;
  }  
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override {}
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override;      
};


class insn_jr : public Insn {
public:
  insn_jr(uint32_t inst, uint64_t addr) :
    Insn(inst, addr, insnDefType::no_dest) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override;
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override;      
};

class insn_jalr : public Insn {
public:
  insn_jalr(uint32_t inst, uint64_t addr) :
    Insn(inst, addr, insnDefType::gpr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;
  void hookupRegs(MipsRegTable<ssaInsn> &tbl) override;    
};





#endif
