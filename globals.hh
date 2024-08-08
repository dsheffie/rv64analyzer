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
  extern bool countInsns;
  extern bool simPoints;
  extern bool replay;
  extern uint64_t simPointsSlice;
  extern uint64_t nCfgCompiles;
  extern region *regionFinder;
  extern basicBlock *cBB;
  extern execUnit *currUnit;
  extern bool enClockFuncts;
  extern bool enableCFG;
  extern bool verbose;
  extern bool ipo;
  extern bool fuseCFGs;
  extern uint64_t nFuses;
  extern uint64_t nAttemptedFuses;
  extern bool enableBoth;
  extern uint32_t enoughRegions;
  extern bool dumpIR;
  extern bool dumpCFG;
  extern bool splitCFGBBs;
  extern std::string blobName;
  extern uint64_t icountMIPS;
  extern std::string binaryName;
  extern std::set<int> openFileDes;
  extern bool profile;
  extern bool log;
};
#endif
