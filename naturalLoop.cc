#include "regionCFG.hh"
#include "naturalLoop.hh"

void naturalLoop::print() const {
  printf("%s ", head->getName().c_str());
  for(cfgBasicBlock *blk : loop) {
    if(blk != head) printf("%s ", blk->getName().c_str());
  }
  printf("\n");
}

/* is other in loop? */
bool naturalLoop::isNestedLoop(const naturalLoop &other) const {
  for(auto bb : other.loop) {
    if(loop.find(bb) == loop.end()) {
      return false;
    }
  }
  return true;
}

bool naturalLoop::isSameLoop(const naturalLoop &other) const {
  if(other.loop.size() != loop.size()) {
    return false;
  }
  for(auto bb : other.loop) {
    if(loop.find(bb) == loop.end()) {
      return false;
    }
  }
  return true;
}
