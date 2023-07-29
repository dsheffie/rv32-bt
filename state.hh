#ifndef __STATE_HH__
#define __STATE_HH__


struct state_t;

#ifndef ELIDE_STATE_IMPL
#include <cstdint>
#include <iostream>

struct state_t{
  uint32_t pc;
  uint32_t oldpc;
  uint32_t last_pc;
  int32_t gpr[32];
  uint64_t abortloc;
  uint8_t *mem;
  uint8_t brk;
  uint8_t bad_addr;
  uint32_t epc;
  uint64_t maxicnt;
  uint64_t icnt;
};


std::ostream &operator<<(std::ostream &out, const state_t & s);
void initState(state_t *s);
#endif

#endif
