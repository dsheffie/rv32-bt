#include <cstdint>     
#include <cstdio>
#include "compile.hh"
#include "riscv.hh"
#define ELIDE_LLVM
#include "globals.hh"
#include <unordered_set>

bool compile::canCompileInstr(uint32_t inst) {
  bool rc = false;
  uint32_t opcode = inst & 127;

  switch(opcode)
    {
    case 0x3:  /* loads */
    case 0xf:  /* fence - there's a bunch of 'em */
    case 0x13: /* reg + imm insns */
    case 0x23: /* stores */      
    case 0x37: /* lui */
    case 0x17: /* auipc */
    case 0x67: /* jalr */
    case 0x6f: /* jal */
    case 0x33:  /* reg + reg insns */     
    case 0x63: /* branches */
      rc = true;
    break;
    case 0x73:
      rc = false;
      break;
    
    default:
      break;
    }
  
  
  return rc;
}
