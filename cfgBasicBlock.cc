#include <ostream>
#include <fstream>

#include "regionCFG.hh"
#include "helper.hh"
#include "globals.hh"

cfgBasicBlock::~cfgBasicBlock() {
  for(size_t i = 0; i < insns.size(); i++) 
    delete insns[i];
  for(size_t i = 0; i < phiNodes.size(); i++) 
    delete phiNodes[i];
}

void cfgBasicBlock::addWithInCFGEdges(regionCFG *cfg) {
  /* if this block has a branch, search if successor
   * is in CFG region */
  iBranchTypeInsn *iBranch = nullptr;
  insn_jr *jr = nullptr;
  insn_jalr *jalr = nullptr;
  insn_j *j = nullptr;
  insn_jal *jal = nullptr;

  std::map<uint64_t,cfgBasicBlock*>::iterator mit0,mit1;
  uint64_t tAddr=~0UL,ntAddr=~0UL;

  /* this is crappy code */
  for(ssize_t i = insns.size()-1; i >= 0; i--) {
    iBranch = dynamic_cast<iBranchTypeInsn*>(insns[i]);
    if(iBranch)
      break;
    jr = dynamic_cast<insn_jr*>(insns[i]);
    if(jr)
      break;
    jal = dynamic_cast<insn_jal*>(insns[i]);
    if(jal)
      break;
    jalr = dynamic_cast<insn_jalr*>(insns[i]);
    if(jalr)
      break;
    j = dynamic_cast<insn_j*>(insns[i]);
    if(j)
      break;
	
    //jJump = dynamic_cast<jTypeInsn*>(insns[i]);
    //if(jJump)
    //break;
    //jrInsn = dynamic_cast<rTypeJumpRegInsn*>(insns[i]);
    //if(jrInsn) break;
  }
  
#define PRINT_ADDED_EDGE(TGT) {\
    std::cerr << __PRETTY_FUNCTION__ << "@" << __LINE__ << " edge : " \
	      << std::hex << getEntryAddr() << " -> " << TGT->getEntryAddr() << std::dec << "\n"; \
  }
  
  if(iBranch) {
    tAddr = iBranch->getTakenAddr();
    ntAddr = iBranch->getNotTakenAddr();
    mit0 = cfg->cfgBlockMap.find(tAddr);
    mit1 = cfg->cfgBlockMap.find(ntAddr);
    if(mit0 != cfg->cfgBlockMap.end()) {
      PRINT_ADDED_EDGE(mit0->second);
      assert(mit0->second->getEntryAddr() == tAddr);
      addSuccessor(mit0->second); 
    }
    if(mit1 != cfg->cfgBlockMap.end()) {
      PRINT_ADDED_EDGE(mit1->second);
      assert(mit1->second->getEntryAddr() == ntAddr);
      addSuccessor(mit1->second); 
    }
  }
  
  //else if(j!=nullptr or jal != nullptr) {
  //tAddr = j ? j->getJumpAddr() : jal->getJumpAddr();
  //mit0 = cfg->cfgBlockMap.find(tAddr);
  //if(mit0 != cfg->cfgBlockMap.end()) {
  //PRINT_ADDED_EDGE(mit0->second);
  //addSuccessor(mit0->second);
  // }
  //}
  
  /* end-of-block with no branch or jump */
  // else {
  //   tAddr = getExitAddr()+4;
  //   mit0 = cfg->cfgBlockMap.find(tAddr);
  //   if(mit0 != cfg->cfgBlockMap.end()) {
  //     PRINT_ADDED_EDGE(mit0->second);
  //     addSuccessor(mit0->second);
  //   }
  // }

}

bool cfgBasicBlock::checkIfPlausableSuccessors() {
  /* find terminating instruction */
  iBranchTypeInsn *iBranch = nullptr;
  for(size_t i = 0,n=insns.size(); i < n; i++) {
    iBranch = dynamic_cast<iBranchTypeInsn*>(insns[i]);
    if(iBranch) 
      break;
  }
  if(!iBranch)
    return true;

  bool impossible = false;


  for(auto cbb : succs) {
    if(cbb->insns.empty())
      continue;
    Insn *ins = cbb->insns[0];
    uint32_t eAddr = ins->getAddr();
    if(iBranch->getTakenAddr() == eAddr || iBranch->getNotTakenAddr() == eAddr)
      continue;
    impossible |= true;
  }
  
  return not(impossible);
}

void cfgBasicBlock::addSuccessor(cfgBasicBlock *s) {
  succs.insert(s);
  s->preds.insert(this);
}

void cfgBasicBlock::delSuccessor(cfgBasicBlock *s) {
  std::set<cfgBasicBlock*>::iterator sit = succs.find(s);
  succs.erase(sit);
  sit = s->preds.find(this);
  s->preds.erase(sit);
}

void cfgBasicBlock::addPhiNode(gprPhiNode *phi) {
  uint32_t r = phi->destRegister();
  if(gprPhis[r])
    delete phi;
  else {
    phiNodes.push_back(phi);
    gprPhis[r] = phi;
  }
}

void cfgBasicBlock::addPhiNode(fprPhiNode *phi) {
  uint32_t r = phi->destRegister();
  if(fprPhis[r])
    delete phi;
  else {
    phiNodes.push_back(phi);
    fprPhis[r] = phi;
  }
}
void cfgBasicBlock::addPhiNode(fcrPhiNode *phi) {
  uint32_t r = phi->destRegister();
  if(fcrPhis[r])
    delete phi;
  else {
    phiNodes.push_back(phi);
    fcrPhis[r] = phi;
  }
}

bool cfgBasicBlock::has_jr_jalr() {
  for(size_t i = 0; i < insns.size(); i++) {
    Insn *ins = insns[i];
    if(dynamic_cast<insn_jr*>(ins))
      return true;
    if(dynamic_cast<insn_jalr*>(ins))
      return true;
  }
  return false;
}


bool cfgBasicBlock::hasFloatingPoint(uint32_t *typeCnts) const {
  bool hasFP = false;
  for(const Insn* I : insns) {
    opPrecType oType = I->getPrecType();
    hasFP |= (oType==singleprec);
    hasFP |= (oType==doubleprec);
    typeCnts[oType]++;
  }
  return hasFP;
}


uint64_t cfgBasicBlock::getExitAddr() const {
  return insns.empty() ? ~(0UL) : insns.at(insns.size()-1)->getAddr();
}

std::string cfgBasicBlock::getName() const {
  return insns.empty() ? std::string("ENTRY") : toStringHex(insns.at(0)->getAddr());
}

std::ostream &operator<<(std::ostream &out, const cfgBasicBlock &bb) {
  out << "Successors of block: ";
  for(auto cBB : bb.succs) {
    if(!cBB->insns.empty()) {
      stream_hex(out, cBB->insns[0]->getAddr());
    }
    else
      out << "ENTRY ";
  }
  out << std::endl;
  out << "Predecessors of block: ";
  for(auto cBB : bb.preds) {
    if(!cBB->insns.empty()) {
      stream_hex(out,cBB->insns[0]->getAddr()); 
    }
    else
      out << "ENTRY ";
  }
  out << std::endl;
  for(auto ins : bb.insns) {
    out << *ins;
  }
  return out;
}


void cfgBasicBlock::print() {
  std::cerr << *this << std::endl;
}


cfgBasicBlock::cfgBasicBlock(basicBlock *bb) :
  bb(bb),
  hasTermBranchOrJump(false),
  lBB(nullptr),
  idombb(nullptr) {
  
  fprTouched.resize(32, fprUseEnum::unused);
  gprPhis.fill(nullptr);
  fprPhis.fill(nullptr);
  fcrPhis.fill(nullptr);
  icntPhis.fill(nullptr);
  

  if(bb) {
    ssize_t numInsns = bb->getVecIns().size();
    for(ssize_t i = 0; i < numInsns; i++) {
	auto &p = bb->getVecIns()[i];
	rawInsns.push_back(p);
    }
  }
}

void cfgBasicBlock::bindInsns(regionCFG *cfg) {
  if(not(insns.empty())) {
    for(size_t i = 0, n=insns.size(); i < n; i++) {
      delete insns[i];
    }
    insns.clear();
  }

  for(const auto & p : rawInsns) {
    Insn *ins = getInsn(p.inst, p.pc);
    assert(ins);
    ins->set(cfg,this);
    insns.push_back(ins);
  }
}


void cfgBasicBlock::updateFPRTouched(uint32_t reg, fprUseEnum useType) {
  const uint32_t mask = ~1U;

  //std::cerr << "reg = " << reg << " new use = " << useType 
  //<< " old type = " << fprTouched[reg] << std::endl;
  
  if(useType == fprUseEnum::doublePrec) {
    switch(fprTouched[reg])
      {
      case fprUseEnum::unused:
	fprTouched[(reg & mask)+0] = fprUseEnum::doublePrec;
	switch(fprTouched[(reg & mask)+1])
	  {
	  case fprUseEnum::unused:
	    fprTouched[(reg & mask)+1] = fprUseEnum::doublePrec;
	    break;
	  case fprUseEnum::singlePrec:
	    fprTouched[(reg & mask)+0] = fprUseEnum::both;
	    fprTouched[(reg & mask)+1] = fprUseEnum::both;
	    break;
	  default:
	    break;
	  }
	break;
      case fprUseEnum::singlePrec:
	fprTouched[(reg & mask)+0] = fprUseEnum::both;
	fprTouched[(reg & mask)+1] = fprUseEnum::both;
	break;
      case fprUseEnum::doublePrec:
      case fprUseEnum::both:
	break;
      default:
	die();
      }
  }
  else if(useType == fprUseEnum::singlePrec) {
    switch(fprTouched[reg])
      {
      case fprUseEnum::unused:
	fprTouched[reg] = fprUseEnum::singlePrec;
	break;
      case fprUseEnum::doublePrec:
	fprTouched[(reg & mask)+0] = fprUseEnum::both;
	fprTouched[(reg & mask)+1] = fprUseEnum::both;
	break;
      case fprUseEnum::singlePrec:
      case fprUseEnum::both:
	break;
      default:
	die();
      }
  }
  else {
    die();
  }
}

bool cfgBasicBlock::dominates(const cfgBasicBlock *B) const {
  if(this==B)
    return true;
  if(B->idombb == this)
    return true;
  if(idombb == B)
    return false;
  
  cfgBasicBlock *P = B->idombb;
  while(P) {
    if(P==this)
      return true;
    else if(P->preds.empty())
      break;
    else
      P = P->idombb;
  }
  return false;
}

llvm::BasicBlock *cfgBasicBlock::getSuccLLVMBasicBlock(uint64_t pc) {
  return nullptr;
}

void cfgBasicBlock::traverseAndRename(regionCFG *cfg){
  /* this only gets called for the entry block */
  ssaRegTables regTbl(cfg);

  assert(ssaInsns.empty());
  
  for(size_t i = 0; i < 32; i++) {
    if(cfg->allGprRead[i] or not(cfg->gprDefinitionBlocks[i].empty())) {
      printf("need to define register %d\n", i);
      ssaInsn *op = new ssaInsn();
      regTbl.gprTbl[i] = op;
      ssaInsns.push_back(op);
      //regTbl.loadGPR(i);
    }
  }
  //for(size_t i = 0; i < 32; i++) {
  //if(cfg->allFprRead[i] or not(cfg->fprDefinitionBlocks[i].empty())) {
  //regTbl.loadFPR(i);
  //}
  //}
  //for(size_t i = 0; i < 5; i++) {
  //if(cfg->allFcrRead[i] or not(cfg->fcrDefinitionBlocks[i].empty())) {
  //  regTbl.loadFCR(i);
  // }
  //}
  
  //termRegTbl.copy(regTbl);
  ssaRegTbl.copy(regTbl);
  
  for(auto nBlock : dtree_succs) {
    nBlock->traverseAndRename(cfg, regTbl);
    break;
  }
  
}

void cfgBasicBlock::patchUpPhiNodes(regionCFG *cfg) {
  for(phiNode* p : phiNodes) {
    for(cfgBasicBlock* bb :  preds) {
      p->addIncomingEdge(cfg, bb);
    }
  }
  
  for(cfgBasicBlock* bb : dtree_succs) {
    bb->patchUpPhiNodes(cfg);
  }
}

void cfgBasicBlock::traverseAndRename(regionCFG *cfg, ssaRegTables prevRegTbl) {
  ssaRegTables regTbl(prevRegTbl);
  
  /* generate code for each instruction */
  for(size_t i = 0, n=insns.size(); i < n; i++) {
    auto insn = insns.at(i);
    insn->hookupRegs(regTbl);
  }

  
  ssaRegTbl.copy(regTbl);
  
  /* walk dominator tree */
  for(auto cbb : dtree_succs){
    /* pre-order traversal */
    cbb->traverseAndRename(cfg, regTbl);
  }
  


}

uint64_t cfgBasicBlock::getEntryAddr() const {
  if(not(rawInsns.empty())) {
    return rawInsns.at(0).pc;
  }
  else if(bb != nullptr) {
    die();
    return bb->getEntryAddr();
  }
  return ~(0UL);
}
