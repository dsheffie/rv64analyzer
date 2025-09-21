#ifndef __naturalLoophh__
#define __naturalLoophh__

class naturalLoop {
private:
  friend class sortNaturalLoops;
  cfgBasicBlock *head;
  std::set<cfgBasicBlock*> loop;
  std::list<naturalLoop*> children;
public:
  naturalLoop(cfgBasicBlock *head, std::set<cfgBasicBlock*> loop) :
    head(head), loop(loop) {}
  bool inSingleBlockLoop(cfgBasicBlock *blk) {
    if(loop.size() != 1)
      return false;
    return (head == blk);
  }
  void addChild(naturalLoop *l) {
    children.push_back(l);
  }
  void print() {
    printf("%s ", head->getName().c_str());
    for(cfgBasicBlock *blk : loop) {
      if(blk != head) printf("%s ", blk->getName().c_str());
    }
    printf("\n");
  }
  bool isNestedLoop(const naturalLoop &other) const;
  bool operator<(const naturalLoop &other) const {
    return loop.size() < other.loop.size();
  }
  const std::set<cfgBasicBlock*> &getLoop() const {
    return loop;
  }
  uint64_t headPC() const {
    return head->getEntryAddr();
  }
  size_t size() const {
    return loop.size();
  }
};

class sortNaturalLoops {
public:
  bool operator() (const naturalLoop *lhs, 
		   const naturalLoop *rhs) const {
    return lhs->loop.size() > rhs->loop.size();
  }
};

#endif
