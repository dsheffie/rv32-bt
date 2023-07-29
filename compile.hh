#ifndef __COMP_INST_HH__
#define __COMP_INST_HH__
#include <cstdint>
class compile {
public:
  static bool canCompileInstr(uint32_t inst);
};
#endif
