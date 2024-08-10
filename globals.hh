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
  extern int sArgc;
  extern char** sArgv;
  extern region *regionFinder;
  extern basicBlock *cBB;
  extern execUnit *currUnit;
  extern bool enClockFuncts;
  extern bool verbose;
  extern bool fuseCFGs;
  extern uint64_t nFuses;
  extern uint64_t nAttemptedFuses;
  extern bool dumpIR;
  extern bool dumpCFG;
  extern std::string blobName;
  extern std::string binaryName;
  extern bool profile;
  extern bool log;
};
#endif
