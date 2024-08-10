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
#include "riscvInstruction.hh"
#include "debugSymbols.hh"

class regionCFG;
class Insn;
class cfgBasicBlock;

#define __fpr_state_list(m) \
  m(unused) \
  m(singlePrec) \
  m(doublePrec) \
  m(both)	\
  m(unknown)

#define __fpr_state_enum(m) m,
enum class fprUseEnum{__fpr_state_list(__fpr_state_enum)};
#undef __fpr_state_enum

inline std::ostream &operator<<(std::ostream &out, const fprUseEnum &fprState) {
#define __fpr_state_pair(m) {fprUseEnum::m, #m},
  std::map<fprUseEnum, std::string> fpr_state_map = {
    __fpr_state_list(__fpr_state_pair)
  };
  return out << fpr_state_map.at(fprState);
#undef __fpr_state_pair
}
#undef __fpr_state_list


class phiNode : public ssaInsn {
 protected:
  std::vector<std::pair<cfgBasicBlock*, ssaInsn*>> inBoundEdges;
 public:
  phiNode(insnDefType insnType = insnDefType::unknown) :
    ssaInsn(insnType) {}
  virtual ~phiNode() {}
  virtual void print() const = 0;
  virtual void addIncomingEdge(regionCFG *cfg, cfgBasicBlock *b)  = 0;
  virtual void hookupRegs(MipsRegTable<ssaInsn> &tbl) override;
  virtual void dump(std::ostream &out) const override;
};

class gprPhiNode : public phiNode {
 protected:
  uint32_t gprId;
 public:
  gprPhiNode(uint32_t gprId) : phiNode(insnDefType::gpr), gprId(gprId){}
  uint32_t destRegister() const override {
    return gprId;
  }
  void addIncomingEdge(regionCFG *cfg, cfgBasicBlock *b) override;
  void print() const override {
    printf("phi for gpr %u\n", gprId);
  }
};

class fprPhiNode : public phiNode {
 protected:
  uint32_t fprId;
  fprUseEnum useType;
 public:
  fprPhiNode(uint32_t fprId, fprUseEnum useType = fprUseEnum::unknown) :
    phiNode(insnDefType::fpr), fprId(fprId), useType(useType) {}
  uint32_t destRegister() const override {
    return fprId;
  }
  void setUseType(fprUseEnum useType) {
    this->useType = useType;
  }
  fprUseEnum getUseType() const {
    return useType;
  }
  void addIncomingEdge(regionCFG *cfg, cfgBasicBlock *b) override;
  bool isFloatingPoint() const override {
    return true;
  }
  void print() const override {
    printf("phi for fpr %u\n", fprId);
  }
};

class fcrPhiNode : public phiNode {
 protected:
  uint32_t fcrId;
 public:
  fcrPhiNode(uint32_t fcrId) : phiNode(insnDefType::fcr), fcrId(fcrId) {}
  uint32_t destRegister() const override {
    return fcrId;
  }
  void addIncomingEdge(regionCFG *cfg, cfgBasicBlock *b) override;
  void print() const override {
    printf("phi for fcr %u\n", fcrId);
  }
};

class ssaRegTables : public MipsRegTable<ssaInsn> {
public:
  regionCFG *cfg = nullptr;
  ssaRegTables(regionCFG *cfg);
  ssaRegTables();
  ssaInsn *loadGPR(uint32_t gpr);
  ssaInsn *setGPR(uint32_t gpr, uint32_t x);
  ssaInsn *loadFPR(uint32_t fpr);
  ssaInsn *loadFCR(uint32_t fcr);
  ssaInsn *getFPR(uint32_t fpr, fprUseEnum useType=fprUseEnum::unused);
  void setFPR(uint32_t fpr, ssaInsn *v);
  void storeGPR(uint32_t gpr);
  void storeFPR(uint32_t fpr);
  void storeFCR(uint32_t fcr);
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
  std::array<phiNode*,32> fprPhis;
  std::array<phiNode*,5> fcrPhis;
  std::array<phiNode*,1> icntPhis;


  std::set<cfgBasicBlock*> preds;
  std::set<cfgBasicBlock*> succs;
  std::set<cfgBasicBlock*> dfrontier;
  std::vector<basicBlock::instruction> rawInsns;
  std::vector<Insn*> insns;
  std::vector<ssaInsn*> ssaInsns;

  
  std::bitset<32> gprRead;
  std::bitset<32> fprRead;
  std::bitset<5> fcrRead;

  std::vector<fprUseEnum> fprTouched;
  
  ssize_t dt_dfn = -1, dt_max_ancestor_dfn = -1;


  void addPhiNode(gprPhiNode *phi);
  void addPhiNode(fprPhiNode *phi);
  void addPhiNode(fcrPhiNode *phi);
  bool checkIfPlausableSuccessors();
  void addWithInCFGEdges(regionCFG *cfg);
  bool has_jr_jalr();
  bool hasFloatingPoint(uint32_t *typeCnts) const;
  void print();
  uint64_t getExitAddr() const;
  uint64_t getEntryAddr() const;
  std::string getName() const;

  void traverseAndRename(regionCFG *cfg);  
  void traverseAndRename(regionCFG *cfg, ssaRegTables regTbl);
  
  void patchUpPhiNodes(regionCFG *cfg);
  void bindInsns(regionCFG *cfg);
  
  void addSuccessor(cfgBasicBlock *s);
  void delSuccessor(cfgBasicBlock *s);
  cfgBasicBlock(basicBlock *bb);
  ~cfgBasicBlock();
  void updateFPRTouched(uint32_t reg, fprUseEnum useType);
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
    return  (this==B) || ((dt_dfn  < B->dt_dfn) && (dt_max_ancestor_dfn >= B->dt_dfn));
  }
  bool dominates(const cfgBasicBlock *B) const;
};

class naturalLoop {
private:
  friend class sortNaturalLoops;
  cfgBasicBlock *head;
  std::set<cfgBasicBlock*> loop;
public:
  naturalLoop(cfgBasicBlock *head, std::set<cfgBasicBlock*> loop) :
    head(head), loop(loop) {}
  bool inSingleBlockLoop(cfgBasicBlock *blk) {
    if(loop.size() != 1)
      return false;
    return (head == blk);
  }
  void print() {
    printf("%s ", head->getName().c_str());
    for(cfgBasicBlock *blk : loop) {
      if(blk != head) printf("%s ", blk->getName().c_str());
    }
    printf("\n");
  }
  bool isNestedLoop(naturalLoop &other) {
    /* Does the head of loop "other" dominate the head 
     * of this loop */
    return head->dominates(other.head);
  }
  bool operator<(const naturalLoop &other) const {
    return loop.size() < other.loop.size();
  }
  const std::set<cfgBasicBlock*> &getLoop() const {
    return loop;
  }
};

class sortNaturalLoops {
public:
  bool operator() (const naturalLoop &lhs, 
		   const naturalLoop &rhs) const {
    return lhs.loop.size() > rhs.loop.size();
  }
};

std::ostream &operator<<(std::ostream &out, const regionCFG &cfg);
class regionCFG : public execUnit {
protected:
  /* definitions */
  const static int histoLen = 8;
  const static bool emitPCs = false;
  /* to be constructor list initialized */
  basicBlock *head = nullptr;
  cfgBasicBlock *cfgHead = nullptr;
  cfgBasicBlock *entryBlock = nullptr;
  cfgBasicBlock *innerPerfectBlock = nullptr;
  uint64_t uuid = 0,runs = 0, minIcnt = 0, maxIcnt = 0;
  double headProb = 0.0;
  bool isMegaRegion = false;
  bool perfectNest = false;
  bool hasBoth = false;
  bool validDominanceAcceleration = false;
  double compileTime = 0.0;
  
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
  bool getEmitPCs() const {
    return emitPCs;
  }

  std::array<uint64_t, histoLen> runHistory;
  std::set<uint64_t> nextPCs;
  std::set<basicBlock*> blocks;
 
  std::set<cfgBasicBlock*> gprDefinitionBlocks[32];
  std::set<cfgBasicBlock*> fprDefinitionBlocks[32];
  std::set<cfgBasicBlock*> fcrDefinitionBlocks[5];

  std::bitset<32> allGprRead;
  std::bitset<32> allFprRead;
  std::bitset<5> allFcrRead;
  std::vector<fprUseEnum> allFprTouched;


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
  regionCFG();
  ~regionCFG();
  bool buildCFG(std::vector<std::vector<basicBlock*> > &regions);

  bool analyzeGraph();
  void dumpIR();
  void emulate(state_t *s);
  void print();
  void asDot() const;
  void findLoop(std::set<cfgBasicBlock*> &loop,
		std::list<cfgBasicBlock*> &stack, 
		cfgBasicBlock *hbb);
  void findNaturalLoops();
  bool dominates(cfgBasicBlock *A, cfgBasicBlock *B) const;
  static void dropCompiled();
  uint64_t getEntryAddr() const override;
  basicBlock* run(state_t *s) override;
  void info() override;
  void toposort(std::vector<cfgBasicBlock*> &topo) const;
  uint64_t countInsns() const;
  uint64_t countBBs() const;
  uint64_t numBBInCommon(const regionCFG &other) const;
};

#endif
