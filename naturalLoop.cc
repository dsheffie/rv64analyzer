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

void naturalLoop::emitGraphviz(int &l_id, std::ostream &out) const {
  out << "subgraph cluster_" << l_id << "{\n";
  out << "label = \"loop_" << l_id << "\"\n";
  for(const auto *bb : getLoop()) {
    out << "\"bb" << std::hex <<  bb->getEntryAddr() <<std::dec << "\"\n";
  }

  l_id++;
  for(auto c : children) {
    c->emitGraphviz(l_id,out);
  }
  
  out << "}\n";

}
