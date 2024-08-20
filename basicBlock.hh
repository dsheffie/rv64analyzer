#ifndef __SIM_BASICBLOCK__
#define __SIM_BASICBLOCK__

#include <set>
#include <vector>
#include <map>
#include <list>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <map>

#include "riscv.hh"
#include "execUnit.hh"

class compile;
class regionCFG;
class basicBlock;

std::ostream &operator<<(std::ostream &out, const basicBlock &bb);

class basicBlock : public execUnit {
public:
  struct instruction {
    uint32_t inst;
    uint64_t pc;
    uint64_t vpc;
    instruction(uint32_t inst, uint64_t pc, uint64_t vpc) :
      inst(inst), pc(pc), vpc(vpc) {}
  };
  typedef std::vector<instruction, backtrace_allocator<instruction>> insContainer;
  static std::map<uint64_t, std::map<uint64_t, uint64_t>> globalEdges;  
private:
  friend std::ostream &operator<<(std::ostream &out, const basicBlock &bb);
  friend int main(int, char**);
  friend class compile;
  friend class region;
  friend class regionCFG;
  struct orderBasicBlocks {
    bool operator() (const basicBlock *a, const basicBlock *b) const {
      return a->getEntryAddr() < b->getEntryAddr();
    }
  };
  static uint64_t cfgCnt;
  static std::map<uint64_t, basicBlock*> bbMap;
  static std::map<uint64_t, basicBlock*> insMap;
  uint64_t entryAddr=0,termAddr = 0;  
  std::set<basicBlock*, orderBasicBlocks> preds,succs;
  std::map<uint32_t, basicBlock *> succsMap;
  bool hasRegion = false;
  std::map<uint32_t, uint32_t> bbRegionCounts; 
  std::vector <std::vector<basicBlock*>>bbRegions;
  regionCFG *cfgCplr = nullptr;
  bool readOnly=false;
  bool hasjr=false, hasjal=false, hasjalr = false, hasmonitor=false;
  uint64_t totalEdges = 0;
  insContainer vecIns;
  std::map<uint64_t, uint64_t> edgeCnts;
  /* pc -> pc and count */
  /* heads of regions that include this block */
  std::set<basicBlock*> cfgInRegions;
  
  void toposort(const std::set<basicBlock*> &valid,
		std::list<basicBlock*> &ordered,
		std::set<basicBlock*> &visited);
public:
  static void dropAllBBs();
  static void dumpCFG();
  void info() override;
  basicBlock* split(uint64_t nEntryAddr);
  void setReadOnly();
  bool isReadOnly() const {
    return readOnly;
  }
  void print() const;
  void repairBrokenEdges();
  ssize_t sizeInBytes() const;
  void addIns(uint32_t inst, uint64_t addr, uint64_t vpc);
  basicBlock(uint64_t entryAddr, basicBlock *prev);
  basicBlock(uint64_t entryAddr);
  ~basicBlock();
  void dropCompiledCode();
  basicBlock *findBlock(uint64_t entryAddr);
  /* no mutate */
  static basicBlock *globalFindBlock(uint64_t entryAddr);
  basicBlock *localFindBlock(uint64_t entryAddr);

  void addSuccessor(basicBlock *bb);
  bool dfs(basicBlock* oldhead, std::set<basicBlock*> &visited, 
	   std::vector<basicBlock*> &path);
  void setCFG(regionCFG *cfg);
  static size_t numBBs() {
    return bbMap.size();
  }
  static size_t numStaticInsns() {
    return insMap.size();
  }
  uint64_t getEntryAddr() const override{
    return entryAddr; 
  }
  uint64_t getTermAddr() const {
    return termAddr;
  }
  void setTermAddr(uint64_t termAddr) {
    if(this->termAddr == 0) {
      this->termAddr = termAddr;
    }
  }
  const insContainer &getVecIns() const {
    return vecIns;
  }
  double edgeWeight(uint64_t pc) const {
    const auto it = edgeCnts.find(pc);
    if(it == edgeCnts.end()) {
      return  0.0;
    }
    return static_cast<double>(it->second) / (totalEdges==0 ? 1 : totalEdges);
  }
  size_t getNumIns() const {
    return vecIns.size();
  }
  bool hasJR(bool isRet=false) const;
  bool hasTermDirectBranchOrJump(uint64_t &target, uint64_t &fallthru) const;
  bool fallsThru() const;
  bool hasJAL() const {
    return hasjal;
  }
  bool hasJALR() const {
    return hasjalr;
  }
  bool hasMONITOR() const {
    return hasmonitor;
  }
  const std::set<basicBlock*, orderBasicBlocks> &getSuccs() const {
    return succs;
  }
  void addToCFGRegions(basicBlock *bb) {
    cfgInRegions.insert(bb);
  }
  bool sanityCheck();
  
  static void toposort(basicBlock *src, const std::set<basicBlock*> &valid, std::list<basicBlock*> &ordered);
};


#endif
