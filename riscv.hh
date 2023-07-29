#ifndef __riscvhh__
#define __riscvhh__


struct utype_t {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t imm : 20;
};

struct branch_t {
  uint32_t opcode : 7;
  uint32_t imm11 : 1; //8
  uint32_t imm4_1 : 4; //12
  uint32_t sel: 3; //15
  uint32_t rs1 : 5; //20
  uint32_t rs2 : 5; //25
  uint32_t imm10_5 : 6; //31
  uint32_t imm12 : 1; //32
};

struct jal_t {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t imm19_12 : 8;
  uint32_t imm11 : 1;
  uint32_t imm10_1 : 10;
  uint32_t imm20 : 1;
};

struct jalr_t {
  uint32_t opcode : 7;
  uint32_t rd : 5; //12
  uint32_t mbz : 3; //15
  uint32_t rs1 : 5; //20
  uint32_t imm11_0 : 12; //32
};

struct rtype_t {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t sel: 3;
  uint32_t rs1 : 5;
  uint32_t rs2 : 5;
  uint32_t special : 7;
};

struct itype_t {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t sel : 3;
  uint32_t rs1 : 5;
  uint32_t imm : 12;
};

struct store_t {
  uint32_t opcode : 7;
  uint32_t imm4_0 : 5; //12
  uint32_t sel : 3; //15
  uint32_t rs1 : 5; //20
  uint32_t rs2 : 5; //25
  uint32_t imm11_5 : 7; //32
};

struct load_t {
  uint32_t opcode : 7;
  uint32_t rd : 5; //12
  uint32_t sel : 3; //15
  uint32_t rs1 : 5; //20
  uint32_t imm11_0 : 12; //32
};

union riscv_t {
  rtype_t r;
  itype_t i;
  utype_t u;
  jal_t j;
  jalr_t jj;
  branch_t b;
  store_t s;
  load_t l;
  uint32_t raw;
  riscv_t(uint32_t x) : raw(x) {}
};

static inline bool is_jr(uint32_t inst) {
  uint32_t opcode = inst & 127;
  uint32_t rd = (inst>>7) & 31;
  return (opcode == 0x67) and (rd == 0);
}

static inline bool is_jalr(uint32_t inst) {
  uint32_t opcode = inst & 127;
  uint32_t rd = (inst>>7) & 31;
  return (opcode == 0x67) and (rd != 0);  
}

static inline bool is_jal(uint32_t inst) {
  uint32_t opcode = inst & 127;
  uint32_t rd = (inst>>7) & 31;
  return (opcode == 0x6f) and (rd != 0);  
}

static inline bool is_j(uint32_t inst) {
  uint32_t opcode = inst & 127;
  uint32_t rd = (inst>>7) & 31;
  return (opcode == 0x6f) and (rd == 0);  
}

static inline bool is_branch(uint32_t inst) {
  uint32_t opcode = inst & 127;
  return (opcode == 0x63);
}

static inline bool isBranchOrJump(uint32_t inst) {
  uint32_t opcode = inst & 127;
  return (opcode == 0x63) or (opcode == 0x6f) or (opcode == 0x67);
}

static inline bool isDirectBranchOrJump(uint32_t inst, uint32_t addr, uint32_t &target) {
  riscv_t m(inst);
  switch(m.b.opcode)
    {
    case 0x63: {
      int32_t disp =
	(m.b.imm4_1 << 1)  |
	(m.b.imm10_5 << 5) |	
        (m.b.imm11 << 11)  |
        (m.b.imm12 << 12);
      disp |= m.b.imm12 ? 0xffffe000 : 0x0;
      target = addr + disp;
      return true;
    }
    case 0x6f: {
      int32_t disp =
	(m.j.imm10_1 << 1)   |
	(m.j.imm11 << 11)    |
	(m.j.imm19_12 << 12) |
	(m.j.imm20 << 20);
      disp |= ((inst>>31)&1) ? 0xffe00000 : 0x0;
      target = addr + disp;
      return true;
    }
    default:
      break;
    }
  return false;
}

static inline bool is_monitor(uint32_t inst) {
  uint32_t opcode = inst & 127;
  if(opcode != 0x73)
    return false;
  if((inst >> 20) != 0)
    return false;
  if( ((inst>>7) & 8191) != 0)
    return false;

  return true;
}

static inline bool isFloatingPoint(uint32_t inst __attribute__((unused))) {
  return false;
}

#endif
