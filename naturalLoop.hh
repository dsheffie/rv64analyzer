#ifndef __naturalLoophh__
#define __naturalLoophh__

class naturalLoop {
private:
  friend class sortNaturalLoops;
  cfgBasicBlock *head, *latch;
  std::set<cfgBasicBlock*> loop;
  std::list<naturalLoop*> children;
public:
  naturalLoop(cfgBasicBlock *head, cfgBasicBlock *latch, std::set<cfgBasicBlock*> loop) :
    head(head), latch(latch), loop(loop) {}
  bool inSingleBlockLoop(cfgBasicBlock *blk) {
    if(loop.size() != 1)
      return false;
    return (head == blk);
  }
  void addChild(naturalLoop *l) {
    children.push_back(l);
  }  
  bool operator<(const naturalLoop &other) const {
    return loop.size() < other.loop.size();
  }
  const std::set<cfgBasicBlock*> &getLoop() const {
    return loop;
  }
  uint64_t headPC() const {
    return head->getEntryAddr();
  }
  uint64_t headVPC() const {
    return head->getEntryVirtualAddr();
  }  
  cfgBasicBlock* getHead() const {
    return head;
  }
  cfgBasicBlock* getLatch() const {
    return latch;
  }
  size_t size() const {
    return loop.size();
  }
  double computeTipCycles() const;
  void print() const;
  bool isNestedLoop(const naturalLoop &other) const;
  bool isSameLoop(const naturalLoop &other) const;
  void emitGraphviz(int &l_id, std::ostream &out) const;
};

class sortNaturalLoops {
public:
  bool operator() (const naturalLoop *lhs, 
		   const naturalLoop *rhs) const {
    return lhs->loop.size() > rhs->loop.size();
  }
};

#endif
