#include <cstdint>     
#include <cstdio>
#include "compile.hh"
#include "riscv.hh"
#define ELIDE_LLVM
#include "globals.hh"
#include <iostream>

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
      return true;
    case 0x63: {/* branches */
      // riscv_t m(inst);
      // switch(m.b.sel)
      // 	{
      // 	case 0: /* beq */
      // 	case 1: /* bne */
      // 	case 4: /* blt */
      // 	case 5: /* bge */
      // 	  return true;
      // 	case 6: /* bltu */
      // 	case 7: /* bgeu */
      // 	  return false;
      // 	  break;
      // 	default:
      // 	  break;
      // 	}      
      return true;
    }
    break;
    case 0x73:
      //std::cout << "opcode stops bt " << std::hex << opcode << std::dec << "\n";
      rc = false;
      break;
    
    default:
      break;
    }
  
  
  return rc;
}
