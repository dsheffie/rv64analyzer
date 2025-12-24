#ifndef __ssainsn_hh__
#define __ssainsn_hh__

#include <array>
#include <set>
#include <list>
#include <vector>
#include "helper.hh"

class cfgBasicBlock;
class regionCFG;

template <typename T>
class MipsRegTable {
public:
  std::array<T*, 32> gprTbl;
public:
  MipsRegTable() {
    gprTbl.fill(nullptr);
  }
  MipsRegTable(const MipsRegTable &other) :
    gprTbl(other.gprTbl) {}
};

enum class insnDefType {no_dest,fpr,gpr,hilo,fcr,icnt,unknown};

class ssaInsn {
protected:
  insnDefType insnType;
  uint64_t uuid;
  std::set<ssaInsn*> uses;
  std::vector<ssaInsn*> sources;
  std::string prettyName;
  static uint64_t uuid_counter;
public:
  ssaInsn(insnDefType insnType = insnDefType::unknown) :
    insnType(insnType), uuid(uuid_counter++) {
  }
  virtual ~ssaInsn() {}    
  const std::string &getName() const {
    return prettyName;
  }
  void makePrettyName();
  void addUse(ssaInsn *u) {
    uses.insert(u);
  }
  const std::set<ssaInsn*>& getUses() const {
    return uses;
  }
  std::set<ssaInsn*>& getUses() {
    return uses;
  }
  const std::vector<ssaInsn*> &getSources() const {
    return sources;
  }
  insnDefType getInsnType() const {
    return insnType;
  }
  void addSrc(ssaInsn *src) {
    sources.push_back(src);
    src->uses.insert(this);
  }
  virtual int32_t destRegister() const {
    return -1;
  }
  virtual bool isFloatingPoint() const {
    return false;
  }
  virtual void hookupRegs(MipsRegTable<ssaInsn> &tbl) {
    die();
  }
  virtual void dumpSSA(std::ostream &out) const {}
  virtual bool isControlFlow() const {
    return false;
  }
  virtual bool isLoad() const {
    return false;
  }
  virtual bool isStore() const {
    return false;
  }
  virtual uint64_t getAddr() const {
    return ~0UL;
  }
};

class phiNode : public ssaInsn {
 protected:
  std::vector<std::pair<cfgBasicBlock*, ssaInsn*>> inBoundEdges;
 public:
  phiNode(insnDefType insnType = insnDefType::unknown) :
    ssaInsn(insnType) {}
  virtual void print() const = 0;
  virtual void addIncomingEdge(regionCFG *cfg, cfgBasicBlock *b)  = 0;
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
  virtual void hookupRegs(MipsRegTable<ssaInsn> &tbl) override;  
  void print() const override {
    printf("phi for gpr %u\n", gprId);
  }
};



#endif
