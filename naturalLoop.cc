#include "regionCFG.hh"
#include "naturalLoop.hh"

void naturalLoop::print() const {
  printf("loop head %s\n", head->getName().c_str());
  for(cfgBasicBlock *blk : loop) {
    if(blk != head) printf("\t%s\n", blk->getName().c_str());
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

double naturalLoop::computeTipCycles() const {
  double c = 0.0;
  for(auto bb : loop) {
    c += bb->computeTipCycles();
  }
  return c;
}
