#include <cassert>
#include "region.hh"
#include "basicBlock.hh"
#include "helper.hh"
#define ELIDE_LLVM 1
#include "globals.hh"

void region::updateRegionHeads(basicBlock *bb, basicBlock *lbb) {}

void region::disableRegionCollection() {}

void region::getRegion(std::vector<basicBlock*> &region) {}

bool region::update(basicBlock *bb) {
  return false;
}


