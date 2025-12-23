#include "basicBlock.hh"
#include "disassemble.hh"
#include "helper.hh"
#include "compile.hh"
#include "regionCFG.hh"
#include <cassert>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <ostream>
#include <fstream>
#include <functional>

#include "globals.hh"

uint64_t basicBlock::cfgCnt = 0;

bool basicBlock::hasJR(bool isRet) const {
    if(isRet) {
      for(const auto & p : vecIns) {
	if(is_jr(p.inst)) {
	  riscv_t m(p.inst);
	  return (m.jj.rs1 == 1) or (m.jj.rs1 == 5);
	}
      }
      return false;
    }
    else {
      return hasjr;
    }
}

void basicBlock::repairBrokenEdges() {
  const ssize_t n_inst = vecIns.size();
  bool found_cflow_insn = false;
  assert(n_inst != 0);
  for(ssize_t i = n_inst - 1; (i >= 0) and not(found_cflow_insn); i--) {
    uint32_t inst = vecIns.at(i).inst;
    if(is_jr(inst)) {
      found_cflow_insn = true;
    }
    else if(is_jal(inst)) {
      found_cflow_insn = true;
    }
    else if(is_j(inst)) {
      found_cflow_insn = true;
    }
    else if(is_branch(inst)) {
      found_cflow_insn = true;
    }
  }
  /* if a basicblock isn't terminated with a branch or jump
   * ensure that it only has one successor */
  if(not(found_cflow_insn) and (succs.size() != 1)) {
    const uint64_t nextpc = vecIns.at(n_inst-1).pc + 4;
    while(succs.size() != 1) {
      for(basicBlock *nbb : succs) {
	if(nbb->getEntryAddr() != nextpc) {
	  auto it0 = succs.find(nbb);
	  succs.erase(it0);
	  auto it1 = nbb->preds.find(this);
	  if(it1 != nbb->preds.end()) {
	    nbb->preds.erase(it1);
	  }
	  break;
	}
      }
    }
  }

  
}

void basicBlock::dumpCFG() {
  std::ofstream out("cfg.txt");
  for(auto &p : bbMap) {
    basicBlock *bb = p.second;
    out << *bb;
  }
  out.close();
}

void basicBlock::dropAllBBs() {
  cfgCnt = 0;
  for(auto &p : bbMap) {
    delete p.second;
  }
  bbMap.clear();
  insMap.clear();
  globals::cBB = nullptr;
}

basicBlock *basicBlock::bbInBlock(uint64_t pc) {
  auto it = insMap.find(pc);
  if(it == insMap.end()) {
    return nullptr;
  }
  return it->second;
}

void basicBlock::setReadOnly() {
  if(not(readOnly)) {
    readOnly = true;
    for(size_t i = 0, len = vecIns.size(); i < len; i++) {
      uint32_t insn = vecIns[i].inst;
      hasjr |= is_jr(insn);
      hasjalr |= is_jalr(insn);
      hasjal |= is_jal(insn);
      hasmonitor |= is_monitor(insn);
    }
    if(cfgCplr) {
      cfgCplr=nullptr;
      delete cfgCplr;
    }
    hasRegion = false;
    bbRegions.clear();
    bbRegionCounts.clear();
  }
 }



bool basicBlock::fallsThru() const {
  bool termIsBranch = isBranchOrJump(vecIns.at(getNumIns()-1).inst);
  return not(termIsBranch);
}


bool basicBlock::mergableWithSucc() const {
  if(not(fallsThru())) {
    return false;
  }
  if(succs.size() != 1) {
    return false;
  }
  auto sbb = *(succs.begin());
  if(sbb->preds.size() != 1) {
    return false;
  }
  return true;
}

void basicBlock::addSuccessor(basicBlock *bb) {
  if(succs.find(bb) != succs.end())
    return;
  
  if(fallsThru() and succs.size() >= 1 and false) {
    std::cout << "can not have more than one successor with fall-thru\n";
    std::cout << "this:\n";
    std::cout << *this;
    std::cout << "bb:\n";    
    std::cout << *bb;
    die();
  }
  
  succs.insert(bb);
  succsMap[bb->entryAddr] = bb;
  bb->preds.insert(this);

}

basicBlock::basicBlock(uint64_t entryAddr) : execUnit(), entryAddr(entryAddr) {
  bbMap[entryAddr] = this;  
}

basicBlock::basicBlock(uint64_t entryAddr, basicBlock *prev) : basicBlock(entryAddr) {
  prev->addSuccessor(this);
}



void basicBlock::addIns(uint32_t inst, uint64_t addr, uint64_t vpc) {
  if(not(readOnly)) {
    vecIns.emplace_back(inst,addr,vpc);
    insMap[addr] = this;
  }
}

void basicBlock::dropCompiledCode() {
  bbRegions.clear();
  bbRegionCounts.clear();
  for(basicBlock *nukeBB : cfgInRegions){
    //std::cout << "delete region " << std::hex << nukeBB->getEntryAddr() << std::dec << "\n";
    if(nukeBB->cfgCplr) {
      delete nukeBB->cfgCplr;
      nukeBB->cfgCplr = nullptr;
      nukeBB->hasRegion = false;
    }
    nukeBB->bbRegions.clear();
    nukeBB->bbRegionCounts.clear();
  }  
}

basicBlock *basicBlock::split(uint64_t nEntryAddr) {
#if 0
  std::cerr << "split @ 0x" << std::hex << entryAddr << std::dec 
	    << " cfgInRegions.size() = " << cfgInRegions.size() 
	    << std::endl;
#endif
  /* dumb linear search because VA != PA */
  ssize_t offs = 0;
  bool found = false;
  for(auto &p : vecIns) {
    if(p.pc == nEntryAddr) {
      found = true;
      break;
    }
    offs++;
  }
  assert(found);
  dropCompiledCode();

  basicBlock *nBB = new basicBlock(nEntryAddr);
  

  //unlink old successors and update with new block
  for(basicBlock *b : succs) {
    auto ssit = b->preds.find(this);
    b->preds.erase(ssit);
    nBB->succsMap[b->entryAddr] = b;
    nBB->succs.insert(b);
    b->preds.insert(nBB);
  }
  //add new successor
  succs.clear();
  succsMap.clear();

  succsMap[nBB->entryAddr] = nBB;
  succs.insert(nBB);
  nBB->preds.insert(this);
  
  for(size_t i = offs, len = vecIns.size(); i < len; i++) {
    uint64_t addr = i*4 + entryAddr;
    insMap[addr] = nBB;
    nBB->vecIns.push_back(vecIns[i]);
  }

  vecIns.erase(vecIns.begin() + offs, vecIns.end());

  nBB->termAddr = termAddr;
  nBB->edgeCnts = edgeCnts;
  nBB->totalEdges = totalEdges;
  edgeCnts.clear();
  edgeCnts[nBB->entryAddr] = totalEdges;

  
  termAddr = nEntryAddr-4;
  readOnly = false;
  hasjr = hasjal = hasjalr = hasmonitor = false;
  setReadOnly();

  nBB->inscnt = inscnt;
  nBB->setReadOnly();
  

  return nBB;
}

basicBlock *basicBlock::globalFindBlock(uint64_t entryAddr) {
  auto it = bbMap.find(entryAddr);
  if(it == bbMap.end())
    return nullptr;
  else
    return it->second;
}

basicBlock *basicBlock::localFindBlock(uint64_t entryAddr) {
  if(entryAddr == this->entryAddr)
    return this;

  const auto it = succsMap.find(entryAddr);
  if(it == succsMap.end())
    return nullptr;
  else
    return it->second;
}

basicBlock *basicBlock::findBlock(uint64_t entryAddr) {
  basicBlock *fBlock = nullptr;
  auto sIt = succsMap.find(entryAddr);
  
  if(sIt != succsMap.end()) {
    fBlock = sIt->second;
  }
  else {
    auto bIt =  bbMap.find(entryAddr);
    if(bIt != bbMap.end()) {
      fBlock = bIt->second;
      addSuccessor(fBlock);
    }
    else {
      auto iIt = insMap.find(entryAddr);
      if(iIt != insMap.end()) {
	basicBlock *sBB = iIt->second;
	basicBlock *nBB = sBB->split(entryAddr);
	fBlock = nBB;
	addSuccessor(fBlock);
      }
    }
  }
  return fBlock;
}

ssize_t basicBlock::sizeInBytes() const {
  if(not(readOnly))
    return -1;
  return 4*vecIns.size();
}

void basicBlock::print() const {
  std::cout << *this;
}

double basicBlock::getTipCycles() const {
  if(cfgCplr == nullptr) {
    return 0.0;
  }
  
  double c = 0.0;
  for(size_t i = 0, l = vecIns.size(); i < l; i++){
    uint64_t addr = entryAddr + i*4;
    c += cfgCplr->getTipCycles(addr);
  }
  return c;
}

std::ostream &operator<<(std::ostream &out, const basicBlock &bb) {
  using namespace std;
  
  out << "block 0x" << hex << bb.entryAddr << dec << " "
      << "cnt = " << bb.inscnt << ","
      << "cycles = " << bb.getTipCycles() << ","
      << "readOnly = " << bb.readOnly << "," 
      << "succs = " << bb.succs.size() << ","
      << "preds = " << bb.preds.size()
      << endl;

  //bb->cfgCplr
  for(size_t i = 0; i < bb.vecIns.size(); i++){
    uint32_t inst = bb.vecIns[i].inst;
    uint64_t addr = bb.entryAddr + i*4;
    string asmString = getAsmString(inst, addr);
    out << hex << addr << dec << " : " << asmString << endl;
    if(addr == 0) {
      cerr << "instruction has address 0, how?\n";
      exit(-1);
    }
  }
  //cfgCplr
  
  out << "successors:";
  for(auto sbb : bb.succs) {
    out << " " << hex << sbb->getEntryAddr() << dec;
  }
  out << "\npredecessors:";
  for(auto pbb : bb.preds) {
    uint64_t t = pbb->getTermAddr();
    uint64_t e = bb.getEntryAddr();
    uint64_t w = basicBlock::globalEdges[t][e];    
    out << " " << hex << pbb->getEntryAddr() << dec << "(" << w << ")";
    
  }
  out << "\nterminal addr = " << hex 
      << bb.termAddr << dec << endl;

  return out;
}




basicBlock::~basicBlock() {
  if(cfgCplr)
    delete cfgCplr;
}
 
void basicBlock::info() {
  std::cout << __PRETTY_FUNCTION__ << " with entry @ "
     	    << std::hex
     	    << getEntryAddr()
     	    << std::dec
	    << "\n";
}


bool basicBlock::dfs(basicBlock* oldhead, std::set<basicBlock*> &visited, 
		     std::vector<basicBlock*> &path) {
  if(visited.find(this)!=visited.end())
    return false;

  visited.insert(this);
  for(basicBlock *nbb : succs) {
    if(nbb==oldhead) {
      path.push_back(this);
      return true;
    }
    if(nbb->dfs(oldhead, visited, path)) {
      path.push_back(this);
      return true;
    }
  }
  return false;
}

void basicBlock::setCFG(regionCFG *cfg) {
  assert(cfgCplr==nullptr);
  cfgCplr = cfg;
  hasRegion = true;
}

void basicBlock::toposort(const std::set<basicBlock*> &valid, std::list<basicBlock*> &ordered, std::set<basicBlock*> &visited) {
  auto it = visited.find(this);
  if(it != visited.end())
    return;
  visited.insert(this);

  for(basicBlock *s : getSuccs()) {
    auto v = valid.find(s);
    if(v == valid.end())
      continue;
    s->toposort(valid, ordered, visited);
  }
  ordered.push_front(this);
}

void basicBlock::toposort(basicBlock *src, const std::set<basicBlock*> &valid, std::list<basicBlock*> &ordered) {
  std::set<basicBlock*> visited;
  src->toposort(valid, ordered, visited);
}

bool basicBlock::hasTermDirectBranchOrJump(uint64_t &target, uint64_t &fallthru) const {
  size_t ni = vecIns.size();
  assert(ni != 0);
  fallthru = vecIns.at(ni-1).pc + 4;
  return isDirectBranchOrJump(vecIns.at(ni-1).inst, vecIns.at(ni-1).pc, target);
  
}

bool basicBlock::sanityCheck() {
  for(ssize_t i = 0, ni = vecIns.size()-1; i < ni; i++) {
    if(isBranchOrJump(vecIns.at(i).inst)) {
      std::cerr << * this;
      die();
    }
  }
  for(const auto sbb : succs) {
    auto it = sbb->preds.find(this);
    if(it == sbb->preds.end()) {
      return false;
    }
  }
  for(const auto pbb : preds) {
    auto it = pbb->succs.find(this);
    if(it == pbb->preds.end()) {
      return false;
    }
  }
  return true;
}
