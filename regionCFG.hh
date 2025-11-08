#ifndef __REGION_CFG_HH__
#define __REGION_CFG_HH__

#include <map>
#include <unordered_map>
#include <set>
#include <bitset>
#include <vector>
#include <list>
#include <array>
#include <cstdint>
#include <limits.h>

#include "execUnit.hh"
#include "basicBlock.hh"
#include "ssaInsn.hh"
#include "pipeline_record.hh"
#include "riscvInstruction.hh"

class regionCFG;
class Insn;
class cfgBasicBlock;
class naturalLoop;

class phiNode : public ssaInsn {
 protected:
  std::vector<std::pair<cfgBasicBlock*, ssaInsn*>> inBoundEdges;
 public:
  phiNode(insnDefType insnType = insnDefType::unknown) :
    ssaInsn(insnType) {}
  virtual void print() const = 0;
  virtual void addIncomingEdge(regionCFG *cfg, cfgBasicBlock *b)  = 0;
  virtual void hookupRegs(MipsRegTable<ssaInsn> &tbl) override;
  virtual void dumpSSA(std::ostream &out) const override;
};

class gprPhiNode : public phiNode {
 protected:
  int32_t gprId;
 public:
  gprPhiNode(uint32_t gprId) : phiNode(insnDefType::gpr), gprId(gprId){}
  int32_t destRegister() const override {
    return gprId;
  }
  void addIncomingEdge(regionCFG *cfg, cfgBasicBlock *b) override;
  void print() const override {
    printf("phi for gpr %u\n", gprId);
  }
};

class ssaRegTables : public MipsRegTable<ssaInsn> {
public:
  regionCFG *cfg = nullptr;
  ssaRegTables(regionCFG *cfg);
  ssaRegTables();
  void copy(const ssaRegTables &other);
};


std::ostream &operator<<(std::ostream &out, const cfgBasicBlock &bb);

class cfgBasicBlock {
 public:
  friend std::ostream &operator<<(std::ostream &out, const cfgBasicBlock &bb);
  friend class regionCFG;
  basicBlock *bb;
  bool hasTermBranchOrJump;
  
  ssaRegTables ssaRegTbl;
  
  cfgBasicBlock *idombb;
  std::set<cfgBasicBlock*> dtree_succs;

  std::vector<phiNode*> phiNodes;
  std::array<phiNode*,32> gprPhis;


  std::set<cfgBasicBlock*> preds;
  std::set<cfgBasicBlock*> succs;
  std::set<cfgBasicBlock*> dfrontier;
  std::vector<basicBlock::instruction> rawInsns;
  std::vector<Insn*> insns;
  std::vector<ssaInsn*> ssaInsns;

  
  std::bitset<32> gprRead;
  
  ssize_t dt_dfn = -1, dt_max_ancestor_dfn = -1;


  void addPhiNode(gprPhiNode *phi);
  void addWithInCFGEdges(regionCFG *cfg);
  bool has_jal_jr_jalr();
  bool hasFloatingPoint(uint32_t *typeCnts) const;
  void print();
  uint64_t getExitAddr() const;
  uint64_t getEntryAddr() const;
  uint64_t getEntryVirtualAddr() const;  
  std::string getName() const;

  void traverseAndRename(regionCFG *cfg);  
  void traverseAndRename(regionCFG *cfg, ssaRegTables regTbl);
  
  void patchUpPhiNodes(regionCFG *cfg);
  void bindInsns(regionCFG *cfg);
  
  void addSuccessor(cfgBasicBlock *s);
  void delSuccessor(cfgBasicBlock *s);
  cfgBasicBlock(basicBlock *bb);
  ~cfgBasicBlock();
  double computeTipCycles() const;
  
  const std::set<cfgBasicBlock*> &getPreds() const {
    return preds;
  }
  size_t numSuccessors() const {
    return succs.size();
  }
  const std::set<cfgBasicBlock*> &getSuccs() const {
    return succs;
  }
  const std::set<cfgBasicBlock*> &getDTSuccs() const {
    return dtree_succs;
  }
  const std::vector<Insn*> &getInsns() const {
    return insns;
  }
  const std::vector<ssaInsn*> &getSSAInsns() const {
    return ssaInsns;
  }
  const basicBlock *getOldBB() const {
    return bb;
  }
  cfgBasicBlock* &getIdom() {
    return idombb;
  }
  uint64_t getOldInscnt() const {
    if(bb) {
      return bb->getInscnt();
    }
    return 0;
  }
  void addDTreeSucc(cfgBasicBlock *bb) {
    dtree_succs.insert(bb);
  }
  bool fastDominates(const cfgBasicBlock *B) const {
    /* Appel exercise 19.1 - constant time dominance */
    return  (this==B) or ((dt_dfn  < B->dt_dfn) and (dt_max_ancestor_dfn >= B->dt_dfn));
  }
  bool dominates(const cfgBasicBlock *B) const;
};



std::ostream &operator<<(std::ostream &out, const regionCFG &cfg);
class regionCFG : public execUnit {
protected:
  std::string name;
  std::map<int64_t, double> &tip;
  std::map<uint64_t,uint64_t> &counts;
  std::list<pipeline_record> &pt;
  std::vector<naturalLoop*> loops,nestedLoops;
  /* to be constructor list initialized */
  basicBlock *head = nullptr;
  cfgBasicBlock *cfgHead = nullptr;
  cfgBasicBlock *entryBlock = nullptr;
  cfgBasicBlock *innerPerfectBlock = nullptr;
  uint64_t uuid = 0;
  bool perfectNest = false;
  bool hasBoth = false;
  bool validDominanceAcceleration = false;
  
 public:
  friend std::ostream &operator<<(std::ostream &out, const regionCFG &cfg);
  static uint64_t icnt;
  static uint64_t iters;
  static std::set<regionCFG*> regionCFGs;

  uint64_t &getuuid() {
    return uuid;
  }
  cfgBasicBlock* &getInnerPerfectBlock() {
    return innerPerfectBlock;
  }
  std::set<uint64_t> nextPCs;
  std::set<basicBlock*> blocks;
 
  std::set<cfgBasicBlock*> gprDefinitionBlocks[32];

  std::bitset<32> allGprRead;
  std::vector< std::vector<naturalLoop> >loopNesting;


  std::vector<cfgBasicBlock*> cfgBlocks;
  std::map<uint64_t, cfgBasicBlock*> cfgBlockMap;

  bool allBlocksReachable(cfgBasicBlock *root);
  void computeDominance();
  void computeDominanceFrontiers();
  void computeLengauerTarjanDominance();
  void fastDominancePreComputation();
  void insertPhis();
  void getRegDefBlocks();
  regionCFG(std::string name, std::map<int64_t, double> &m,
	    std::map<uint64_t,uint64_t> &c, std::list<pipeline_record> &r);
  ~regionCFG();
  bool buildCFG(std::vector<std::vector<basicBlock*> > &regions);

  bool analyzeGraph();
  void dumpIR();
  void dumpRISCV();  
  void print();
  void asDot() const;
  void asText() const;
  void findLoop(std::set<cfgBasicBlock*> &loop,
		std::list<cfgBasicBlock*> &stack, 
		cfgBasicBlock *hbb);
  void findNaturalLoops();
  void printNaturalLoops(int d = 0) const;
  bool dominates(cfgBasicBlock *A, cfgBasicBlock *B) const;
  uint64_t getEntryAddr() const override;
  void info() override;
  void toposort(std::vector<cfgBasicBlock*> &topo) const;
  uint64_t countInsns() const;
  uint64_t countBBs() const;
  uint64_t numBBInCommon(const regionCFG &other) const;
  double getTipCycles(uint64_t ip) const;
};

#endif
