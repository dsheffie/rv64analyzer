#include <queue>
#include <algorithm>
#include <ostream>
#include <limits>
#include <fstream>
#include <boost/dynamic_bitset.hpp>
#include <fcntl.h>

#include "regionCFG.hh"
#include "helper.hh"
#include "disassemble.hh"
#include "globals.hh"
#include "interpret.hh"

static regionCFG *currCFG = nullptr;

/* Implementation from Muchnick and Lengauer-Tarjan TOPLAS 
 * paper. Vague understanding from Appel. */
class LengauerTarjanDominators {
private:
  std::vector<cfgBasicBlock*> &blocks;
  cfgBasicBlock *root = nullptr;
  ssize_t dfsCounter = 1;
  /* Parent in the DFS tree */
  std::map<cfgBasicBlock*, cfgBasicBlock*> Parent;
  /* Ancestor chain in DFS tree */
  std::map<cfgBasicBlock*, cfgBasicBlock*> Ancestor;
  std::map<cfgBasicBlock*, cfgBasicBlock*> Label;
  std::map<cfgBasicBlock*, std::set<cfgBasicBlock*>> Buckets;
  std::map<ssize_t, cfgBasicBlock*> Ndfs;
  std::map<cfgBasicBlock*, ssize_t> Sdno;
  
  void DFS(cfgBasicBlock *bb) {
    Sdno.at(bb) = dfsCounter;
    Ndfs[dfsCounter] = bb;
    Label[bb] = bb;
    Ancestor[bb] = nullptr;
    dfsCounter++;
    for(cfgBasicBlock *nbb : bb->getSuccs()) {
      if(Sdno.at(nbb) == 0) {
	Parent[nbb] = bb;
	DFS(nbb);
      }
    }
  }
  /* these methods are from Lengauer-Tarjan paper
   * and implement O(n*lg(n)) scheme */
  void Link(cfgBasicBlock *v, cfgBasicBlock *w) {
    Ancestor.at(w) = v;
  }
  void Compress(cfgBasicBlock *bb) {
    if(Ancestor.at(Ancestor.at(bb))) {
      Compress(Ancestor.at(bb));
      if(Sdno.at(Label.at(Ancestor.at(bb))) < Sdno.at(Label.at(bb))) {
	Label.at(bb) = Label.at(Ancestor.at(bb));
      }
      Ancestor.at(bb) = Ancestor.at(Ancestor.at(bb));
    }
  }
  cfgBasicBlock *Eval(cfgBasicBlock *bb) {
    if(!Ancestor.at(bb)) {
      return bb;
    }
    else {
      Compress(bb);
      return Label.at(bb);
    }
  }
  
public:
  LengauerTarjanDominators(std::vector<cfgBasicBlock*> &blocks) : blocks(blocks) {}
  void operator()() {
    for(cfgBasicBlock *bb : blocks) {
      if(bb->getPreds().empty()) {
	assert(root==nullptr);
	root = bb;
      }
      Sdno[bb] = 0;
      Parent[bb] = nullptr;
      bb->getIdom() = nullptr;
    }
    assert(root);
    DFS(root);
    
    for(ssize_t i = (dfsCounter-1); i > 1; i--) {
      cfgBasicBlock *w = Ndfs.at(i);
      ssize_t &sdom_w = Sdno.at(w);
      for(cfgBasicBlock *pbb : w->getPreds()) {
	cfgBasicBlock *u = Eval(pbb);
	ssize_t sdom_u = Sdno.at(u);
	if(sdom_u < sdom_w) {
	  sdom_w = sdom_u;
	}
      }
      Buckets[Ndfs.at(sdom_w)].insert(w);
      Link(Parent.at(w), w);
      /* need to understand - why parent bucket? */
      std::set<cfgBasicBlock*> &pBucket = Buckets[Parent.at(w)];
      for(cfgBasicBlock *v : pBucket) {
	/* find ancestor with lowest semidominator */
	cfgBasicBlock *u = Eval(v);
	if(Sdno.at(u) < Sdno.at(v)) {
	  /* idom is the semidominator */
	  v->getIdom() = u;
	}
	else {
	  /* must defer */
	  v->getIdom() = Parent.at(w);
	}
      }
      pBucket.clear();
    }
    for(ssize_t i = 2; i < dfsCounter; i++) {
      cfgBasicBlock *w = Ndfs.at(i);
      if(w->getIdom() != Ndfs.at(Sdno.at(w))) {
	w->getIdom() = w->getIdom()->getIdom();
      }
    }
    for(cfgBasicBlock *bb : blocks) {
      if(bb->getPreds().empty()) {
	continue;
      }
      bb->getIdom()->addDTreeSucc(bb);
    }
  }
  
};


template <typename T>
class sortByIcnt {
public:
  bool operator() (const T& lhs, const T& rhs) const {
    return lhs->getInscnt() > rhs->getInscnt();
  }
};


template <typename T>
void inducePhis(const std::set<cfgBasicBlock*> &defBBs, int id) {
  std::list<cfgBasicBlock*> workList;
  std::set<cfgBasicBlock*> checkSet;
  for(cfgBasicBlock* cbb : defBBs) { 
    workList.push_back(cbb); 
    checkSet.insert(cbb); 
  }
  while(not(workList.empty()))  {
    cfgBasicBlock *cbb = workList.front();
    workList.pop_front();
    for(cfgBasicBlock* dbb : cbb->dfrontier) {
      T* phi = new T(id);
      dbb->addPhiNode(phi);
      if(checkSet.find(dbb) == checkSet.end()) {
	checkSet.insert(dbb);
	workList.push_back(dbb);
      }
    }
  }
}


ssaRegTables::ssaRegTables(regionCFG *cfg) :
  MipsRegTable<ssaInsn>(),
  cfg(cfg) {}

ssaRegTables::ssaRegTables() :
  MipsRegTable<ssaInsn>(),
  cfg(nullptr){}

void ssaRegTables::copy(const ssaRegTables &other) {
  cfg = other.cfg;
  gprTbl = other.gprTbl;
}

void regionCFG::getRegDefBlocks() {
  for(auto cbb : cfgBlocks) {
    for(size_t i = 0, n = cbb->insns.size(); i < n; i++) {
      Insn *ins = cbb->insns[i];
      ins->recDefines(cbb, this);
      ins->recUses(cbb);
    }
    /* Union bitvectors */
    for(size_t i = 0; i < 32; i++) {
      allGprRead[i] = allGprRead[i] or cbb->gprRead[i];
    }
  }
}

bool regionCFG::allBlocksReachable(cfgBasicBlock *root) {
  std::queue<cfgBasicBlock*> q;
  std::set<cfgBasicBlock*> v;
  std::list<cfgBasicBlock*> l;
  q.push(root);
  while(not(q.empty())) {
    cfgBasicBlock *cbb = q.front();
    q.pop();
    if(v.find(cbb) != v.end())
      continue;
    v.insert(cbb);
    l.push_back(cbb);
    for(auto nbb : cbb->succs) {
      q.push(nbb);
    }
  }

  for(auto bb : cfgBlocks) {
    if(v.find(bb) == v.end()) {
      std::cout << "couldnt find block "
		<< std::hex
		<< bb->getEntryAddr()
		<< std::dec
		<< "\n";
    }
  }
  /*
  print_var(v.size());
  print_var(cfgBlocks.size());
  */
  return v.size() == cfgBlocks.size();
}


bool regionCFG::buildCFG(std::vector<std::vector<basicBlock*> > &regions) {
  std::set<basicBlock*> heads;
  std::map<basicBlock*, cfgBasicBlock*> cfgMap;
  std::vector<basicBlock*> blockvec;
  std::set<basicBlock*> discovered; 
  std::list<basicBlock*> visited;
  std::set<basicBlock*> seen;
  
  currCFG = this;

  for(size_t i = 0, n = regions.size(); i < n; i++) {
    heads.insert(regions[i][0]);
    for(size_t j = 0, nn=regions[i].size(); j < nn; j++) {
      basicBlock *bb = regions[i][j];
      if(bb==nullptr) die();
      blocks.insert(bb);
    }
  }

  
  if(heads.size()!=1) {
    printf("something has gone horribly wrong: %zu region heads in cfg\n", 
	   heads.size());
    die();
  }
  printf("%d\n", __LINE__);
  head = regions.at(0).at(0);
  printf("%d\n", __LINE__);  
  if(head==nullptr) {
    die();
  }

  
  std::cerr << "regionCFG block @ 0x"
	    << std::hex << head->getEntryAddr()
	    << std::dec
	    << " with " << blocks.size()
	    << " basicblocks\n";

  blockvec.reserve(blocks.size());
  for(auto bb : blocks) {
    blockvec.push_back(bb);
  }
  std::sort(blockvec.begin(), blockvec.end(), sortByIcnt<basicBlock*>());
   
  for(auto bb : blocks) {
    assert(bb->sanityCheck());
  }  
  
  
  for(auto bb : blocks) {
    cfgBasicBlock *cbb = new cfgBasicBlock(bb);
    if(bb == head) {
      cfgHead = cbb;
    }
    cfgBlocks.push_back(cbb);
    cfgMap[bb] = cbb;
  }
  
  printf("%d\n", __LINE__);
  for(auto bb : blocks) {
    for(auto nbb : bb->getSuccs()) {
      auto it = cfgMap.find(nbb);
      if(it != cfgMap.end()) {
	cfgMap[bb]->addSuccessor(it->second);
      }
    }
  }
  printf("%d\n", __LINE__);
 

  /* "compile" mips instructions into proper class */
  for(size_t i = 0; i < cfgBlocks.size(); i++) {
    cfgBasicBlock *cbb = cfgBlocks[i];
    uint64_t ep = cbb->getEntryAddr();
    cbb->bindInsns(this);
    auto it = cfgBlockMap.find(ep);
    assert(it == cfgBlockMap.end());
    cfgBlockMap[ep] = cbb;
  }

  printf("%d\n", __LINE__);
  uint32_t typeCnts[dummyprec-integerprec] = {0};
  for(size_t i = 0; i < cfgBlocks.size(); i++) {
    cfgBasicBlock *cbb = cfgBlocks[i];
    cbb->hasFloatingPoint(typeCnts);
  }

  if(not(allBlocksReachable(cfgMap[head]))) {
    die();
  }

  
  bool rc = analyzeGraph();
  if(not(rc) and globals::verbose) {
    std::cout << "COMPILE FAILED  in analysis\n";
  }
  return rc;
}

bool regionCFG::analyzeGraph() {
  entryBlock = new cfgBasicBlock(nullptr);
  cfgBlocks.push_back(entryBlock);
  entryBlock->addSuccessor(cfgHead);
  
  if(cfgBlocks.size() < 512) {
    computeDominance();
  }
  else {
    computeLengauerTarjanDominance();
  }

  fastDominancePreComputation();
  
  computeDominanceFrontiers();
  /* search for natural loops */
  findNaturalLoops();
  
  /* "compile" mips instructions into proper class */
  for(size_t i = 0, n = cfgBlocks.size(); i < n; i++) {
    cfgBasicBlock *cbb = cfgBlocks[i];
    cbb->bindInsns(this);
  }

  /* insert phis into basicblocks */
  insertPhis();

  uint32_t cnt = 0;
  for(size_t i = 0; i < 32; i++)
    cnt = allGprRead[i] ? cnt + 1 : cnt;

  
  entryBlock->traverseAndRename(this);
  entryBlock->patchUpPhiNodes(this);

 
  dumpIR();
  dumpRISCV();
  asDot();
  
  return true;
}

regionCFG::regionCFG() : execUnit() {
  regionCFGs.insert(this);
  perfectNest = true;
  isMegaRegion = false;
  innerPerfectBlock = 0;
  runs = uuid = 0;
  minIcnt = std::numeric_limits<uint64_t>::max();
  maxIcnt = 0; 
  headProb = 0.0;
  head = nullptr;
  cfgHead = nullptr;
  entryBlock = 0;
  hasBoth = false;
  validDominanceAcceleration = false;
  runHistory.fill(0);
}
regionCFG::~regionCFG() {
  regionCFGs.erase(regionCFGs.find(this));
  
  for(auto cblk : cfgBlocks) {
    delete cblk;
  }
  cfgBlocks.clear();
}

 
void regionCFG::insertPhis()  {
  /* find blocks where registers are "defined" */
  getRegDefBlocks();
  /* if any register is written in the cfg */
  if(not(gprDefinitionBlocks[0].empty())) {
    std::cout << "writing to the zero reg?\n";
  }
  //assert(gprDefinitionBlocks[0].empty());
  
  for(size_t gpr = 0; gpr < 32; gpr++) {
    if(not(gprDefinitionBlocks[gpr].empty())) {
      gprDefinitionBlocks[gpr].insert(entryBlock);
    }
  }
  /* handle gprs */
  for(size_t gpr = 1; gpr < 32; gpr++) {
    inducePhis<gprPhiNode>(gprDefinitionBlocks[gpr], gpr);
  }
}

void regionCFG::fastDominancePreComputation() {
  using namespace std;
  size_t cnt = 0;
  /* run DFS on the dominator tree */
  function<void(cfgBasicBlock*)> dfs =[&](cfgBasicBlock* bb) {
    cnt++;
    /* pre-order - get dfnum */
    bb->dt_dfn = cnt;
    for(auto nbb : bb->getDTSuccs()) {
      /* dom-tree .. no cycles */
      dfs(nbb);
    }
    /* post-order - max dfnum of successors */
    bb->dt_max_ancestor_dfn = cnt;
  };
  dfs(entryBlock);

  validDominanceAcceleration = true;

#if 0
  for(size_t i = 0, n = cfgBlocks.size(); i < n; i++) {
    auto A = cfgBlocks.at(i);
    for(size_t j = i; j < n; j++) {
      auto B = cfgBlocks.at(j);
      bool d0 = A->fastDominates(B);
      bool d1 = A->dominates(B);
      assert(d0 == d1);
    }
  }
#endif
}

void regionCFG::computeLengauerTarjanDominance() {
  LengauerTarjanDominators LTD(cfgBlocks);
  LTD();
  
}
 
void regionCFG::computeDominance() {
  using namespace boost;
  using namespace std;
  bool changed = true;
  vector<cfgBasicBlock*> preOrderVisit(cfgBlocks.size());
  std::map<cfgBasicBlock*, size_t> blockToNumMap;
  
  map<cfgBasicBlock*, dynamic_bitset<>> domMap;
  
  size_t dfsNum = 0;

  /* use DFS to compute DFS numbers */
  function<void(cfgBasicBlock*)> dfs =[&](cfgBasicBlock* bb) {
    preOrderVisit.at(dfsNum) = bb;
    blockToNumMap[bb] = dfsNum;
    dfsNum++;
    domMap[bb] = dynamic_bitset<>(cfgBlocks.size());
    domMap.at(bb).set();
    for(auto nbb : bb->succs) {
      auto it = blockToNumMap.find(nbb);
      if(it == blockToNumMap.end()) {
	dfs(nbb);
      }
    }
  };
  
  /* initialize */
  dfs(entryBlock);

  domMap.at(entryBlock).reset();
  domMap.at(entryBlock)[0] = true;

  
  /* compute dominators */
  do {
    changed = false;
    for(size_t bId=0, n=preOrderVisit.size(); bId < n; bId++) {
      cfgBasicBlock *cbb = preOrderVisit[bId];
      dynamic_bitset<> tdd = domMap.at(cbb);
      for(cfgBasicBlock *nbb : cbb->getPreds()) {
	tdd &= domMap.at(nbb);
      }
      tdd[bId] = true;
      //check if changed
      if(tdd != domMap.at(cbb)) {
	changed = true;
	domMap.at(cbb) = tdd;
      }
    }
  }
  while(changed);
  
  for(size_t i = 1, n = preOrderVisit.size(); i < n; i++) {
    cfgBasicBlock *cbb = preOrderVisit[i];
    dynamic_bitset<> &dom = domMap.at(cbb);
    dom[i] = false;
    /* iterate over blocks that dominate current block */
    for(size_t j = dom.find_first(); j != dynamic_bitset<>::npos; j = dom.find_next(j)) {
      const dynamic_bitset<> &ddom = domMap.at(preOrderVisit.at(j));
      for(size_t k = ddom.find_first(); k != dynamic_bitset<>::npos; k = ddom.find_next(k)) {
	/* if j dominates k, k can not be the immediate dominator */
	if(k!=j)
	  dom[k]=false;
      }
    }
    cbb->getIdom() = preOrderVisit.at(dom.find_first());
    cbb->getIdom()->addDTreeSucc(cbb);
  }
  entryBlock->getIdom() = nullptr;
}
 
void regionCFG::computeDominanceFrontiers() {
  /* compute dominance frontiers */
  for(auto cbb : cfgBlocks) {
    /* only blocks with more than one pred
     * can update dominance frontiers */
    if(cbb->preds.size() > 1) {
      for(auto nbb : cbb->getPreds()) {
	/* iterate back to idom 
	 * adding this node to 
	 * appropriate dominance frontiers */
	while((nbb != cbb->getIdom())) {
	    nbb->dfrontier.insert(cbb);
	    nbb = nbb->getIdom();
	  }
      }
    }
  }
}

bool regionCFG::dominates(cfgBasicBlock *A, cfgBasicBlock *B) const {
  if(validDominanceAcceleration) {
    return A->fastDominates(B);
  }
  else {
    return A->dominates(B);
  }
}


void phiNode::hookupRegs(MipsRegTable<ssaInsn> &tbl) {}

void phiNode::dumpSSA(std::ostream &out) const {
  out << this;
  out << "<- ";
  for(auto p : inBoundEdges) {
    auto bb = p.first;
    auto ins = p.second;
    out << "(" <<
      std::hex <<
      bb->getEntryAddr() <<
      std::dec <<
      "," << ins << ")" << " ";
  }

}




void gprPhiNode::addIncomingEdge(regionCFG *cfg, cfgBasicBlock *b) {
  assert(b!=nullptr);
  
  ssaInsn *in = b->ssaRegTbl.gprTbl[gprId];
  assert(in);

  inBoundEdges.emplace_back(b, in);
}


void regionCFG::asDot() const {
  const std::string filename = "cfg_" + toStringHex(head->getEntryAddr()) + ".dot"; 
  std::ofstream out(filename);
  std::set<const basicBlock*> bbs;
  
  for(const cfgBasicBlock* cbb : cfgBlocks) {
    const basicBlock *bb = cbb->bb;
    if(bb) {
      bbs.insert(bb);
    }
  }
  
  out << "digraph G {\n";
  /* vertices */
  
  for(const auto bb : bbs) {
    const auto & insns = bb->getVecIns();
    uint32_t ea = bb->getEntryAddr();
    out << "\"bb" << std::hex << ea << std::dec << "\"[\n";
    out << "label = <bb_0x" << std::hex << ea << std::dec
	<< " : " << "<BR align='left'/>";
    for(ssize_t i = 0, ni = insns.size(); i < ni; i++) {
      const auto &p = insns.at(i);
      uint32_t inst = p.inst, addr = i*4 + ea;
      auto asmString = getAsmString(inst, addr);
      out << std::hex << addr << std::dec
	  << " : " << asmString << "<BR align='left'/>";
     
    }
    out << ">\nshape=\"record\"\n];\n";
  }
  /* edges */
  for(const auto* bb : bbs) {
    std::stringstream ss;
    ss << "\"bb" << std::hex << bb->getEntryAddr() << std::dec << "\"";
    std::string s = ss.str();
    for(const auto &nbb : bb->getSuccs()) {
      out << s
	  << " -> "
	  << "\"bb"
	  << std::hex
	  << nbb->getEntryAddr()
	  << std::dec
	  << "\"\n"; 
    }
  }
  
  out << "}\n";
  out.close();
}

void regionCFG::print() {
  std::cerr << *this << std::endl;
}


std::ostream &operator<<(std::ostream &out, const regionCFG &cfg) {
  std::vector<cfgBasicBlock*> topo;
  cfg.toposort(topo);
  for(size_t i = 0, n = topo.size(); i < n; i++) {
    const cfgBasicBlock *bb = topo.at(i);
    out << "block 0x" << std::hex << bb->getEntryAddr()
       << std::dec << " : ";
    for(const cfgBasicBlock *nbb : bb->getSuccs()) {
      out << std::hex << nbb->getEntryAddr()
         << std::dec << " ";
    }
    out << "\n";
    const auto &c = bb->rawInsns;
    for(size_t j = 0, nb = c.size(); j < nb; j++) {
      auto inst = c.at(j).inst;
      auto addr = c.at(j).pc;
      out << "\t" << std::hex << addr << std::dec << " : ";      
      disassemble(out,inst,addr);
      out << "\n";
    }
  }
  return out;
}

void regionCFG::dumpRISCV() {
  std::stringstream ss;
  ss << "cfg_" << std::hex << cfgHead->getEntryAddr() << std::dec << ".txt";
  std::ofstream o(ss.str());
  o << *this;
  o.close();
}

void regionCFG::dumpIR() {
  std::stringstream ss;
  ss << "ssa_" << std::hex << cfgHead->getEntryAddr() << std::dec << ".txt";
  std::ofstream out(ss.str());

  std::vector<cfgBasicBlock*> topo;
  toposort(topo);
  for(size_t i = 0, n = topo.size(); i < n; i++) {
    const cfgBasicBlock *bb = topo.at(i);
    out << "block 0x" << std::hex << bb->getEntryAddr()
       << std::dec << " : ";
    for(const cfgBasicBlock *nbb : bb->getSuccs()) {
      out << std::hex << nbb->getEntryAddr()
         << std::dec << " ";
    }
    out << "\n";
    for(auto ins : bb->ssaInsns) {
      ins->dumpSSA(out);
      out << "\n";
    }
  }
  
  //out << *this;
  out.close();
}
 

void regionCFG::findLoop(std::set<cfgBasicBlock*> &loop, 
			std::list<cfgBasicBlock*> &stack,
			cfgBasicBlock *hbb) {
  while(!stack.empty()) {
    cfgBasicBlock *c = stack.front();
    stack.pop_front();
    loop.insert(c);
    if(c != hbb) {
      for(std::set<cfgBasicBlock*>::iterator sit = c->preds.begin(); sit != c->preds.end(); sit++) {
	cfgBasicBlock *cc = *sit;
	if(loop.find(cc) == loop.end()) {	
	  stack.push_front(cc);
	}
      }
    }
  }
}


void regionCFG::findNaturalLoops() {
  std::vector<naturalLoop> loops;
  std::map<cfgBasicBlock*, std::vector<naturalLoop> >loopHeadMap;
  for(cfgBasicBlock *lbb : cfgBlocks) {
    for(cfgBasicBlock *hbb : lbb->succs) {
      if(dominates(hbb, lbb)) {
	std::set<cfgBasicBlock*> loop;
	std::list<cfgBasicBlock*> stack;
	stack.push_front(lbb);
	/* BFS on preds to find natural loops */
	findLoop(loop, stack, hbb);
	naturalLoop l(hbb,loop);
	//loops.push_back(l);
	loopHeadMap[hbb].push_back(l);
      }
    }
  }

  for(std::map<cfgBasicBlock*, std::vector<naturalLoop> >::iterator mit = loopHeadMap.begin();
      mit != loopHeadMap.end(); mit++) {
    cfgBasicBlock *hbb = mit->first;
    std::set<cfgBasicBlock*> loop;
    if(mit->second.size() > 1)
      perfectNest = false;
    for(std::vector<naturalLoop>::iterator vit = mit->second.begin(); vit != mit->second.end(); vit++) {
      for(auto cfgbb : vit->getLoop()) {
	loop.insert(cfgbb);
      }
    }
    naturalLoop l(hbb,loop);
    loops.push_back(l);
  }
  if(loops.empty())
    return;
  
  std::sort(loops.begin(), loops.end(), sortNaturalLoops());

  std::vector<std::vector<size_t> > nests(loops.size());
  std::vector<int> depths(loops.size());
  std::fill(depths.begin(), depths.end(), 0);

  /* Building subset matrix */
  for(ssize_t i = loops.size()-1; i >= 0; i--) {
    for(ssize_t j = i - 1; j >= 0; j--) {
      /* Is loop i nested in loop(nest) j? */
      if(loops[i].isNestedLoop(loops[j])){
	nests[j].push_back(i);
	break;
      }
    }
  }

  //for(size_t i = 0; i < nests.size(); i++) {
  //printf("nest %zu: ", i);
  // for(size_t j = 0; j < nests[i].size(); j++) {
  //  printf("%zu ", nests[i][j]);
  // }
  // printf("\n");
  //}
  
  /* Traverse nesting tree using BFS 
   * to find max depth */
  std::queue<std::pair<size_t, int> > q;
  q.push(std::pair<size_t,int>(0,1));
  int maxDepth = 1;
  while(!q.empty()) {
    std::pair<size_t, int> p= q.front();
    /* assign loop to proper depth */
    depths[p.first] = p.second;
    q.pop();
    for(size_t i = 0; i < nests[p.first].size(); i++) {
      int d = p.second+1;
      maxDepth = std::max(maxDepth, d);
      std::pair<size_t,int> pp(nests[p.first][i], d);
      q.push(pp);
    }
  }

  
  //for(size_t i = 0; i < nests.size(); i++) {
  // printf("nest (depth=%d) %zu: ", i, depths[i]);
  //for(size_t j = 0; j < nests[i].size(); j++) {
  //  printf("%zu ", nests[i][j]);
  // }
  // printf("\n");
  //}



  /*printf("maxDepth = %d\n", maxDepth); */
  std::vector< std::vector<naturalLoop> > loopN(maxDepth+1);
  
  for(size_t i = 0; i < loops.size(); i++) {
    int d = depths[i];
    loopN[d].push_back(loops[i]);
  }
  

  for(size_t i = 1; i < loopN.size(); i++) {
    perfectNest &= (loopN[i].size()==1);
  }

  //for(size_t i = 1; i < loopN.size(); i++) {
  // printf("%zu\n", loopN[i].size());
  // for(size_t j = 0; j < loopN[i].size(); j++) {
  //   printf("depth %zu : ", i); loopN[i][j].print();
  // }
  //}

  //printf("head %x %s a perfect nest\n", 
  //head->getEntryAddr(), perfectNest ? "is" : "isnt" );
    
  loopNesting = loopN;
}
 
uint64_t regionCFG::getEntryAddr() const {
  return head ? head->getEntryAddr() : ~0UL;
}

void regionCFG::info() {
  std::cout << __PRETTY_FUNCTION__ << " with entry @ "
	    << std::hex
	    << getEntryAddr()
	    << std::dec
	    << "\n";
  std::cout << *this << "\n";
}

uint64_t regionCFG::countInsns() const {
  uint64_t cnt = 0;
  for(const basicBlock* bb : blocks) {
    cnt += bb->getNumIns();
  }
  return cnt;
}

uint64_t regionCFG::countBBs() const {
  return blocks.size();
}

uint64_t regionCFG::numBBInCommon(const regionCFG &other) const {
  bool other_larger = other.blocks.size() > blocks.size();
  uint64_t common = 0;
  const auto &large = other_larger ? other.blocks : blocks;
  const auto &small = other_larger ? blocks : other.blocks;

  for(const auto &bb : small) {
    auto it = large.find(bb);
    if(it != large.end()) {
      common++;
    }
  }
  
  return common;
}
 
void regionCFG::toposort(std::vector<cfgBasicBlock*> &topo) const {
  std::set<cfgBasicBlock*> visited;
  std::function<void(cfgBasicBlock*)> dfs = [&](cfgBasicBlock *bb) {
    assert(bb != nullptr);
    if(visited.find(bb) != visited.end())
      return;
    visited.insert(bb);
    for(auto nbb : bb->getSuccs()) {
      dfs(nbb);
    }
    topo.push_back(bb);
  };
  dfs(entryBlock);
  std::reverse(topo.begin(), topo.end());
}

