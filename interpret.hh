#ifndef __INTERPRET_HH__
#define __INTERPRET_HH__

#include <cstdint>
#include <list>

#include "inst_record.hh"


void buildCFG(const std::list<inst_record> &trace, std::map<uint64_t,uint64_t> &counts);


#endif
