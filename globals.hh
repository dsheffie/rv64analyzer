#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <string.h>
#include <string>
#include <set>
#include <map>

class region;
class basicBlock;
class execUnit;

namespace globals {
  extern std::string templatePath;
  extern region *regionFinder;
  extern basicBlock *cBB;
  extern execUnit *currUnit;
  extern bool enClockFuncts;
  extern bool verbose;
  extern bool dumpIR;
  extern bool dumpCFG;
};
#endif
