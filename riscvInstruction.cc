#include <cstdio>
#include <cstdlib>
#include "disassemble.hh"
#include "riscvInstruction.hh"
#include "regionCFG.hh"
#include "helper.hh"
#include "globals.hh"

typedef llvm::Value lv_t;

std::ostream &operator<<(std::ostream &out, const Insn &ins) {
  out << "0x" << std::hex << ins.addr << std::dec 
      << " : " << getAsmString(ins.inst, ins.addr) 
      << std::endl;
  return out;
}

class loadInsn : public Insn {
public:
  loadInsn(uint32_t inst, uint32_t addr) : Insn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {
    cfg->gprDefinitionBlocks[r.l.rd].insert(cBB);
  }
  void recUses(cfgBasicBlock *cBB) override {
    cBB->gprRead[r.l.rs1]=true;
  }
};

class storeInsn : public Insn {
public:
  storeInsn(uint32_t inst, uint32_t addr) :
    Insn(inst, addr,insnDefType::no_dest) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {
    cBB->gprRead[r.s.rs1]=true;
    cBB->gprRead[r.s.rs2]=true;
  }
};


/* iType */
class insn_beq : public iBranchTypeInsn {
 public:
 insn_beq(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};

class insn_bne : public iBranchTypeInsn {
 public:
 insn_bne(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};


class insn_blt : public iBranchTypeInsn {
 public:
  insn_blt(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};

class insn_bge : public iBranchTypeInsn {
public:
  insn_bge(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};

class insn_bltu : public iBranchTypeInsn {
 public:
  insn_bltu(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};

class insn_bgeu : public iBranchTypeInsn {
public:
  insn_bgeu(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};

class insn_lui : public Insn {
 public:
 insn_lui(uint32_t inst, uint32_t addr) :
   Insn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override {
    llvm::LLVMContext &cxt = *(cfg->Context);
    llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
    regTbl.gprTbl[r.u.rd] = llvm::ConstantInt::get(iType32,inst&0xfffff000);
    return false;
  }
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {
    cfg->gprDefinitionBlocks[r.u.rd].insert(cBB);
  }  
};

class insn_auipc : public Insn {
 public:
 insn_auipc(uint32_t inst, uint32_t addr) : Insn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override {
    llvm::LLVMContext &cxt = *(cfg->Context);
    llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
    uint32_t imm = inst & (~4095U);
    uint32_t u = static_cast<uint32_t>(addr) + imm;
    regTbl.gprTbl[r.u.rd] = llvm::ConstantInt::get(iType32,u);
    return false;
  }
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {
    cfg->gprDefinitionBlocks[r.u.rd].insert(cBB);
  }
  
};


class insn_lb : public loadInsn {
 public:
 insn_lb(uint32_t inst, uint32_t addr) : loadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
 
};

class insn_lh : public loadInsn {
 public:
 insn_lh(uint32_t inst, uint32_t addr) : loadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};

class insn_lw : public loadInsn {
 public:
 insn_lw(uint32_t inst, uint32_t addr) : loadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};

class insn_lbu : public loadInsn {
 public:
 insn_lbu(uint32_t inst, uint32_t addr) : loadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};

class insn_lhu : public loadInsn {
 public:
 insn_lhu(uint32_t inst, uint32_t addr) : loadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};


class insn_sb : public storeInsn {
 public:
 insn_sb(uint32_t inst, uint32_t addr) : storeInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
 
};

class insn_sh : public storeInsn {
 public:
 insn_sh(uint32_t inst, uint32_t addr) : storeInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
 
};

class insn_sw : public storeInsn {
 public:
  insn_sw(uint32_t inst, uint32_t addr) : storeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};



std::string Insn::getString() const {
  std::stringstream ss;
  ss << *this;
  return ss.str();
}

bool Insn::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl){
  std::cout << getAsmString(inst,addr) << " unimplemented\n";
  die();
  return false;
}


void Insn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
}

void Insn::recUses(cfgBasicBlock *cBB) {
}


void Insn::set(regionCFG *cfg, cfgBasicBlock *cBB) {
  this->cfg = cfg;  myBB = cBB;
}

void Insn::emitPrintPC() {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  llvm::Value *vAddr = llvm::ConstantInt::get(iType32,addr);
  std::vector<llvm::Value*> argVector;
  argVector.push_back(vAddr);
  llvm::ArrayRef<llvm::Value*> cArgs(argVector);
  cfg->myIRBuilder->CreateCall(cfg->builtinFuncts["print_pc"],cArgs);
}

void Insn::saveInstAddress() {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType64 = llvm::Type::getInt64Ty(cxt);
  llvm::Value *vZ = llvm::ConstantInt::get(iType64,0);
  llvm::Value *vAddr = llvm::ConstantInt::get(iType64,(uint64_t)addr);
  llvm::Value *vG = cfg->myIRBuilder->MakeGEP(cfg->blockArgMap["icnt"], vZ);
  cfg->myIRBuilder->CreateStore(vAddr, vG);
}

void Insn::codeGen(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  generateIR(cBB, regTbl);
}


/* r-type */
void rTypeInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[r.r.rd].insert(cBB);
}

void rTypeInsn::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[r.r.rs1]=true;
  cBB->gprRead[r.r.rs2]=true;
}

void insn_jalr::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[r.r.rd].insert(cBB);
}

void insn_jalr::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[r.r.rs1]=true;  
}
void insn_jr::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[r.r.rs1]=true;
}


bool iBranchTypeInsn::handleBranch(cfgBasicBlock *cBB,
				   llvmRegTables& regTbl,
				   llvm::Value *vCMP) {



  llvm::BasicBlock *tBB = cBB->getSuccLLVMBasicBlock(tAddr);
  llvm::BasicBlock *ntBB = cBB->getSuccLLVMBasicBlock(ntAddr);
  if(tAddr != ntAddr) {
    llvm::BasicBlock *t0 = cfg->generateAbortBasicBlock(tAddr, regTbl,cBB,tBB,addr);
    llvm::BasicBlock *t1 = ntBB = cfg->generateAbortBasicBlock(ntAddr,regTbl,cBB,ntBB,addr);
    tBB = t0;
    ntBB = t1;
    cfg->myIRBuilder->CreateCondBr(vCMP, tBB, ntBB);
  }
  else {
    tBB = cfg->generateAbortBasicBlock(tAddr, regTbl,cBB,tBB,addr);
    cfg->myIRBuilder->CreateBr(tBB);
  }
  
  cBB->hasTermBranchOrJump = true;

  return true;

}
void iTypeInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[r.i.rd].insert(cBB);
}

void iTypeInsn::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[r.i.rs1]=true;
}

void iBranchTypeInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  /* don't update anything.. */
}
void iBranchTypeInsn::recUses(cfgBasicBlock *cBB) {
  /* branches read both operands */
  cBB->gprRead[r.b.rs1]=true;
  cBB->gprRead[r.b.rs2]=true;
}

void insn_jal::recDefines(cfgBasicBlock *cBB, regionCFG *cfg)  {
  uint32_t rd = (inst>>7) & 31;
  assert(rd != 0);
  cfg->gprDefinitionBlocks[rd].insert(cBB);
}


Insn* getInsn(uint32_t inst, uint32_t addr){
  uint32_t opcode = inst & 127;
  uint32_t rd = (inst>>7) & 31;
  riscv_t m(inst);

  switch(opcode)
    {
    case 0x3:  /* loads */
      switch(m.r.sel)
	{
	case 0x0: /* lb */
	  return new insn_lb(inst, addr);
	case 0x1: /* lh */
	  return new insn_lh(inst, addr);
	case 0x2: /* lw */
	  return new insn_lw(inst, addr);
	case 0x4: /* lbu */
	  return new insn_lbu(inst, addr);
	case 0x5:  /* lhu */
	  return new insn_lhu(inst, addr);	  
	}
      break;
    case 0xf:  /* fence - there's a bunch of 'em */
      break;
    case 0x13: /* reg + imm insns */
      return new iTypeInsn(inst, addr);
    case 0x23: {/* stores */
      switch(m.s.sel)
	{
	case 0x0: /* sb */
	  return new insn_sb(inst, addr);
	case 0x1: /* sh */
	  return new insn_sh(inst, addr);
	case 0x2: /* sw */
	  return new insn_sw(inst, addr);
	default:
	  break;
	}
      break;
    }
    case 0x37: /* lui */
      return new insn_lui(inst, addr);
    case 0x17: /* auipc */
      return new insn_auipc(inst, addr);
    case 0x67: {/* jalr */
      return (rd==0) ? dynamic_cast<Insn*>(new insn_jr(inst, addr)) :
	dynamic_cast<Insn*>(new insn_jalr(inst,addr));
    }
    case 0x6f: {/* jal */
      return (rd==0) ? dynamic_cast<Insn*>(new insn_j(inst, addr)) :
	dynamic_cast<Insn*>(new insn_jal(inst,addr));
    }
    case 0x33:  /* reg + reg insns */
      return new rTypeInsn(inst, addr);
    case 0x63: /* branches */
      switch(m.b.sel)
	{
	case 0: /* beq */
	  return new insn_beq(inst, addr);
	case 1: /* bne */
	  return new insn_bne(inst, addr);
	case 4: /* blt */
	  return new insn_blt(inst, addr);	  
	case 5: /* bge */
	  return new insn_bge(inst, addr);	  	  
	case 6: /* bltu */
	  return new insn_bltu(inst, addr);	  
	case 7: /* bgeu */
	  return new insn_bgeu(inst, addr);	  	  
	default:
	  break;
	}      
      return nullptr;
    
    default:
      break;
    }
  
  return nullptr;
}




bool iTypeInsn::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  using namespace llvm;
  LLVMContext &cxt = *(cfg->Context);
  Type *iType32 = Type::getInt32Ty(cxt);
  Value *vRD = nullptr;

  int32_t simm32 = (inst >> 20);
  simm32 |= ((inst>>31)&1) ? 0xfffff000 : 0x0;
  uint32_t uimm32 = static_cast<uint32_t>(simm32);
  uint32_t subop =(inst>>12)&7;
  uint32_t shamt = (inst>>20) & 31;
  auto vSIMM32 = ConstantInt::get(iType32,simm32);
  auto vUIMM32 = ConstantInt::get(iType32,uimm32);
  
  switch(r.i.sel)
    {
    case 0: /* addi */
      vRD = cfg->myIRBuilder->CreateAdd(regTbl.gprTbl[r.i.rs1], vSIMM32);
      break;
    case 1: { /* slli */
      auto vSA = ConstantInt::get(iType32, shamt);
      vRD = cfg->myIRBuilder->CreateShl(regTbl.gprTbl[r.i.rs1], vSA);
      break;
    }
    case 2: { /* slti */
      llvm::Value *vZ = llvm::ConstantInt::get(iType32,0);
      llvm::Value *vO = llvm::ConstantInt::get(iType32,1);
      llvm::Value *vRS = regTbl.gprTbl[r.i.rs1];
      llvm::Value *vCMP = cfg->myIRBuilder->CreateICmpSLT(vRS, vSIMM32);
      vRD = cfg->myIRBuilder->CreateSelect(vCMP, vO, vZ);
      break;
    }
    case 3: { /* sltiu */
      llvm::Value *vZ = llvm::ConstantInt::get(iType32,0);
      llvm::Value *vO = llvm::ConstantInt::get(iType32,1);
      llvm::Value *vRS = regTbl.gprTbl[r.i.rs1];
      llvm::Value *vCMP = cfg->myIRBuilder->CreateICmpULT(vRS, vUIMM32);
      vRD = cfg->myIRBuilder->CreateSelect(vCMP, vO, vZ);
      break;
    }
    case 4: /* xori */
      vRD = cfg->myIRBuilder->CreateXor(regTbl.gprTbl[r.i.rs1], vSIMM32);
      break;
    case 5: { /* srli & srai */
      uint32_t sel =  (inst >> 25) & 127;
      auto vSA = ConstantInt::get(iType32, shamt);
      if(sel == 0) { /* srli */
	vRD = cfg->myIRBuilder->CreateLShr(regTbl.gprTbl[r.i.rs1], vSA);
      }
      else if(sel == 32) { /* srai */
	vRD = cfg->myIRBuilder->CreateAShr(regTbl.gprTbl[r.i.rs1], vSA);
      }
      else {
	die();
      }
      break;
    }
    case 6: /* ori */
      vRD = cfg->myIRBuilder->CreateOr(regTbl.gprTbl[r.i.rs1], vSIMM32);
      break;
    case 7: /* andi */
      vRD = cfg->myIRBuilder->CreateAnd(regTbl.gprTbl[r.i.rs1], vSIMM32);
      break;
      
    default:
      std::cout << "implement case " << subop << "\n";
      assert(false);
    }
  
  regTbl.gprTbl[r.i.rd] = vRD;
  return false;
}

bool rTypeInsn::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  using namespace llvm;
  LLVMContext &cxt = *(cfg->Context);
  Value *vRD = nullptr;
  Type *iType32 = Type::getInt32Ty(cxt);
  Value* vM5 = ConstantInt::get(iType32,0x1f);
  Value *vZ = ConstantInt::get(iType32,0);
  Value *vO = ConstantInt::get(iType32,1);

  switch(r.r.sel)
    {
    case 0x0:
      switch(r.r.special)
	{
	case 0x0: /* add */
	  vRD = cfg->myIRBuilder->CreateAdd(regTbl.gprTbl[r.r.rs1], regTbl.gprTbl[r.r.rs2]);
	  break;
	case 0x1: /* mul */
	  vRD = cfg->myIRBuilder->CreateMul(regTbl.gprTbl[r.r.rs1], regTbl.gprTbl[r.r.rs2]);
	  break;
	case 0x20: /* sub */
	  vRD = cfg->myIRBuilder->CreateSub(regTbl.gprTbl[r.r.rs1], regTbl.gprTbl[r.r.rs2]);	  
	  break;
	default:
	  std::cout << "sel = " << r.r.sel << ", special = " << r.r.special << "\n";
	  std::cout << " : " << getAsmString(inst, addr) << "\n";
	  assert(0);
	}
      break;
    case 0x1:
      switch(r.r.special)
	{
	case 0x0: /* sll */
	  vRD = cfg->myIRBuilder->CreateAnd(regTbl.gprTbl[r.r.rs2], vM5);
	  vRD = cfg->myIRBuilder->CreateShl(regTbl.gprTbl[r.r.rs1], vRD);
	  break;
	case 0x1: {/* mulu */
	  llvm::Type *iType64 = llvm::Type::getInt64Ty(cxt);
	  llvm::Value *vRS = cfg->myIRBuilder->CreateSExt(regTbl.gprTbl[r.r.rs1], iType64);
	  llvm::Value *vRT = cfg->myIRBuilder->CreateSExt(regTbl.gprTbl[r.r.rs2], iType64);
	  llvm::Value *vMul = cfg->myIRBuilder->CreateMul(vRS,vRT);
	  llvm::Value *v32 = llvm::ConstantInt::get(iType64,32);
	  llvm::Value *vShft = cfg->myIRBuilder->CreateAShr(vMul, v32);	  
	  vRD = cfg->myIRBuilder->CreateTrunc(vShft, iType32);
	  break;
	}
	default:	  
	  die();
	}
      break;
    case 0x2: /* slt */
      switch(r.r.special)
	{
	case 0x0:
	  vRD = cfg->myIRBuilder->CreateICmpSLT(regTbl.gprTbl[r.r.rs1],regTbl.gprTbl[r.r.rs2]);
	  vRD = cfg->myIRBuilder->CreateSelect(vRD, vO, vZ);	  
	  break;
	default:
	  die();
	}
      break;
    case 0x3:
      switch(r.r.special)
	{
	case 0x0: /* sltu */
	  vRD = cfg->myIRBuilder->CreateICmpULT(regTbl.gprTbl[r.r.rs1],regTbl.gprTbl[r.r.rs2]);
	  vRD = cfg->myIRBuilder->CreateSelect(vRD, vO, vZ);
	  break;
	case 0x1: {/* mulhu */
	  llvm::Type *iType64 = llvm::Type::getInt64Ty(cxt);
	  llvm::Value *vZRS = cfg->myIRBuilder->CreateZExt(regTbl.gprTbl[r.r.rs1], iType64);
	  llvm::Value *vZRT = cfg->myIRBuilder->CreateZExt(regTbl.gprTbl[r.r.rs2], iType64);
	  llvm::Value *vMul = cfg->myIRBuilder->CreateMul(vZRS,vZRT);
	  llvm::Value *v32 = llvm::ConstantInt::get(iType64,32);
	  llvm::Value *vShft = cfg->myIRBuilder->CreateLShr(vMul, v32);	  
	  vRD = cfg->myIRBuilder->CreateTrunc(vShft, iType32);
	  break;
	}
	default:
	  die();
	}
      break;
    case 0x4:
      switch(r.r.special)
	{
	case 0x0:
	  vRD = cfg->myIRBuilder->CreateXor(regTbl.gprTbl[r.r.rs1], regTbl.gprTbl[r.r.rs2]);
	  break;
	case 0x1: {
	  llvm::Value *vOne = llvm::ConstantInt::get(iType32,1);
	  llvm::Value *vZero = llvm::ConstantInt::get(iType32,0);
	  llvm::Value *vCmp = cfg->myIRBuilder->CreateICmpEQ(regTbl.gprTbl[r.r.rs2], vZero);
	  llvm::Value *vDivider = cfg->myIRBuilder->CreateSelect(vCmp, vOne, regTbl.gprTbl[r.r.rs2]);
	  vRD = cfg->myIRBuilder->CreateSDiv(regTbl.gprTbl[r.r.rs1], vDivider);
	  break;
	}
	default:
	  die();
	}
      break;
    case 0x5:
      switch(r.r.special)
	{
	case 0x0:
	  vRD = cfg->myIRBuilder->CreateAnd(regTbl.gprTbl[r.r.rs2], vM5);
	  vRD = cfg->myIRBuilder->CreateLShr(regTbl.gprTbl[r.r.rs1], vRD);
	  break;
	case 0x1: {
	  llvm::Value *vOne = llvm::ConstantInt::get(iType32,1);
	  llvm::Value *vZero = llvm::ConstantInt::get(iType32,0);
	  llvm::Value *vCmp = cfg->myIRBuilder->CreateICmpEQ(regTbl.gprTbl[r.r.rs2], vZero);
	  llvm::Value *vDivider = cfg->myIRBuilder->CreateSelect(vCmp, vOne, regTbl.gprTbl[r.r.rs2]);
	  vRD = cfg->myIRBuilder->CreateUDiv(regTbl.gprTbl[r.r.rs1], vDivider);
	  break;
	}
	case 0x20:
	  vRD = cfg->myIRBuilder->CreateAnd(regTbl.gprTbl[r.r.rs2], vM5);
	  vRD = cfg->myIRBuilder->CreateAShr(regTbl.gprTbl[r.r.rs1], vRD);
	  break;
	  
	default:
	  die();
	}
      break;
    case 0x6:
      switch(r.r.special)
	{
	case 0x0:
	  vRD = cfg->myIRBuilder->CreateOr(regTbl.gprTbl[r.r.rs1], regTbl.gprTbl[r.r.rs2]);
	  break;
	case 0x1: {
	  llvm::Value *vOne = llvm::ConstantInt::get(iType32,1);
	  llvm::Value *vZero = llvm::ConstantInt::get(iType32,0);
	  llvm::Value *vCmp = cfg->myIRBuilder->CreateICmpEQ(regTbl.gprTbl[r.r.rs2], vZero);
	  llvm::Value *vDivider = cfg->myIRBuilder->CreateSelect(vCmp, vOne, regTbl.gprTbl[r.r.rs2]);
	  vRD = cfg->myIRBuilder->CreateSRem(regTbl.gprTbl[r.r.rs1], vDivider);	  
	  break;
	}
	default:
	  die();
	}
      break;
    case 0x7:
      switch(r.r.special)
	{
	case 0x0:
	  vRD = cfg->myIRBuilder->CreateAnd(regTbl.gprTbl[r.r.rs1], regTbl.gprTbl[r.r.rs2]);
	  break;
	case 0x1: {
	  llvm::Value *vOne = llvm::ConstantInt::get(iType32,1);
	  llvm::Value *vZero = llvm::ConstantInt::get(iType32,0);
	  llvm::Value *vCmp = cfg->myIRBuilder->CreateICmpEQ(regTbl.gprTbl[r.r.rs2], vZero);
	  llvm::Value *vDivider = cfg->myIRBuilder->CreateSelect(vCmp, vOne, regTbl.gprTbl[r.r.rs2]);
	  vRD = cfg->myIRBuilder->CreateURem(regTbl.gprTbl[r.r.rs1], vDivider);
	  break;
	}	  
	default:
	  die();
	}
      break;
    default:
      std::cout << "rtype " << r.r.sel <<  " unhandled\n";
      die();
      break;
    }
  assert(vRD);
  regTbl.gprTbl[r.r.rd] = vRD;
  return false;
}


bool insn_sb::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  int32_t disp = r.s.imm4_0 | (r.s.imm11_5 << 5);
  disp |= ((inst>>31)&1) ? 0xfffff000 : 0x0;
  
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*(cfg->Context)),disp);
  llvm::Value *vRS = regTbl.gprTbl[r.s.rs1];
  llvm::Value *vRT = regTbl.gprTbl[r.s.rs2];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA = cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(*(cfg->Context)));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->MakeGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP,llvm::Type::getInt8PtrTy(*(cfg->Context)));
  llvm::Value *vTrunc = cfg->myIRBuilder->CreateTrunc(vRT, llvm::Type::getInt8Ty(*(cfg->Context)));
  cfg->myIRBuilder->CreateStore(vTrunc, vPtr);
  return false;
}

bool insn_sh::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  int32_t disp = r.s.imm4_0 | (r.s.imm11_5 << 5);
  disp |= ((inst>>31)&1) ? 0xfffff000 : 0x0;
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*(cfg->Context)),disp);
  llvm::Value *vRS = regTbl.gprTbl[r.s.rs1];
  llvm::Value *vRT = regTbl.gprTbl[r.s.rs2];

  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA = cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(cxt));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->MakeGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt16PtrTy(cxt));
  llvm::Value *vTrunc = cfg->myIRBuilder->CreateTrunc(vRT, llvm::Type::getInt16Ty(cxt));
  cfg->myIRBuilder->CreateStore(vTrunc, vPtr);
  return false;
}



bool insn_sw::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  int32_t disp = r.s.imm4_0 | (r.s.imm11_5 << 5);
  disp |= ((inst>>31)&1) ? 0xfffff000 : 0x0;
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*(cfg->Context)),disp);
  llvm::Value *vRS = regTbl.gprTbl[r.s.rs1];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA = cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(cxt));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->MakeGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt32PtrTy(cxt));
  cfg->myIRBuilder->CreateStore(regTbl.gprTbl[r.s.rs2], vPtr);
  return false;
}


bool insn_lbu::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) { 
  int32_t disp = r.l.imm11_0;
  if((inst>>31)&1) {
    disp |= 0xfffff000;
  }
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*(cfg->Context)),disp);
  llvm::Value *vRS = regTbl.gprTbl[r.l.rs1];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA =  cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(*(cfg->Context)));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->MakeGEP(vMem, vZEA);
  std::string loadName = "lbu_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vLoad = cfg->myIRBuilder->MakeLoad(vGEP,loadName);
  llvm::Value *vSext = cfg->myIRBuilder->CreateZExt(vLoad, llvm::Type::getInt32Ty(*(cfg->Context)));
  regTbl.gprTbl[r.l.rd] = vSext;
  return false;
}

bool insn_lb::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  int32_t disp = r.l.imm11_0;
  if((inst>>31)&1) {
    disp |= 0xfffff000;
  }  
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  llvm::Type *iType64 = llvm::Type::getInt64Ty(cxt);
  llvm::Value *vIMM = llvm::ConstantInt::get(iType32,disp);
  llvm::Value *vRS = regTbl.gprTbl[r.l.rs1];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA = cfg->myIRBuilder->CreateZExt(vEA, iType64);
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->MakeGEP(vMem, vZEA);
  std::string loadName = "lb_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vLoad = cfg->myIRBuilder->MakeLoad(vGEP,loadName);
  regTbl.gprTbl[r.l.rd] = cfg->myIRBuilder->CreateSExt(vLoad,iType32);
  return false;
}

bool insn_lh::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  int32_t disp = r.l.imm11_0;
  if((inst>>31)&1) {
    disp |= 0xfffff000;
  }    
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*(cfg->Context)),disp);
  llvm::Value *vRS = regTbl.gprTbl[r.l.rs1];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA =  cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(*(cfg->Context)));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->MakeGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt16PtrTy(*(cfg->Context)));
  std::string loadName = "lh_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vLoad = cfg->myIRBuilder->MakeLoad(vPtr,loadName);
  llvm::Value *vSext = cfg->myIRBuilder->CreateSExt(vLoad,  
						    llvm::Type::getInt32Ty(*(cfg->Context)));
  regTbl.gprTbl[r.l.rd] = vSext;
  return false;
}

bool insn_lhu::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  int32_t disp = r.l.imm11_0;
  if((inst>>31)&1) {
    disp |= 0xfffff000;
  }      
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  llvm::Type *iType64 = llvm::Type::getInt64Ty(cxt);
  llvm::Value *vIMM = llvm::ConstantInt::get(iType32, disp);
  llvm::Value *vRS = regTbl.gprTbl[r.l.rs1];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA =  cfg->myIRBuilder->CreateZExt(vEA, iType64);
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->MakeGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt16PtrTy(cxt));
  std::string loadName = "lhu_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vLoad = cfg->myIRBuilder->MakeLoad(vPtr,loadName);
  regTbl.gprTbl[r.l.rd] = cfg->myIRBuilder->CreateZExt(vLoad, iType32);
  return false;
}

bool insn_lw::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
    int32_t disp = r.l.imm11_0;
  if((inst>>31)&1) {
    disp |= 0xfffff000;
  }    
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*(cfg->Context)),disp);
  llvm::Value *vRS = regTbl.gprTbl[r.l.rs1];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA =  cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(*(cfg->Context)));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->MakeGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt32PtrTy(*(cfg->Context)));
  std::string loadName = "lw_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  regTbl.gprTbl[r.l.rd] = cfg->myIRBuilder->MakeLoad(vPtr,loadName); 
  return false;
}

bool insn_bltu::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmp(llvm::CmpInst::ICMP_ULT, regTbl.gprTbl[r.b.rs1], regTbl.gprTbl[r.b.rs2]);
  return handleBranch(cBB, regTbl,vCMP);
}

bool insn_blt::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmp(llvm::CmpInst::ICMP_SLT, regTbl.gprTbl[r.b.rs1], regTbl.gprTbl[r.b.rs2]);  
  return handleBranch(cBB,regTbl,vCMP);
}

bool insn_bne::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmp(llvm::CmpInst::ICMP_NE, regTbl.gprTbl[r.b.rs1], regTbl.gprTbl[r.b.rs2]);    
  return handleBranch(cBB,regTbl,vCMP);
}


bool insn_beq::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmp(llvm::CmpInst::ICMP_EQ, regTbl.gprTbl[r.b.rs1], regTbl.gprTbl[r.b.rs2]);      
  return handleBranch(cBB,regTbl,vCMP);
}


bool insn_bge::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmp(llvm::CmpInst::ICMP_SGE, regTbl.gprTbl[r.b.rs1], regTbl.gprTbl[r.b.rs2]);        
  return handleBranch(cBB,regTbl,vCMP);
}

bool insn_bgeu::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmp(llvm::CmpInst::ICMP_UGE, regTbl.gprTbl[r.b.rs1], regTbl.gprTbl[r.b.rs2]);          
  return handleBranch(cBB,regTbl,vCMP);
}


bool insn_j::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  int32_t jaddr =
    (r.j.imm10_1 << 1)   |
    (r.j.imm11 << 11)    |
    (r.j.imm19_12 << 12) |
    (r.j.imm20 << 20);
  jaddr |= ((inst>>31)&1) ? 0xffe00000 : 0x0;
  jaddr += addr;
  llvm::BasicBlock *tBB = cBB->getSuccLLVMBasicBlock(jaddr);

  if(tBB==nullptr) {
    std::cerr << "COULDNT FIND 0x" << std::hex << jaddr
	      << " from 0x" << addr
	      << std::dec << "\n";
    std::cerr << *cfg;
    die();
  }
  cfg->myIRBuilder->CreateBr(tBB);
  cBB->hasTermBranchOrJump = true;
  return true;
}

bool insn_jal::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {  
  llvm::LLVMContext &cxt = *(cfg->Context);
  uint32_t rd = (inst>>7) & 31;
  assert(rd != 0);
  regTbl.gprTbl[rd] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cxt),(addr+4));
  cfgBasicBlock *nBB = *(cBB->succs.begin());
  cfg->myIRBuilder->CreateBr(nBB->lBB);
  cBB->hasTermBranchOrJump = true;
  return true;
}

bool insn_jalr::canCompile() const {
  return true;
}

bool insn_jr::canCompile() const {
  return true;
}


bool insn_jr::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  std::vector<llvm::BasicBlock*> fallT(cBB->succs.size() + 1);
  std::vector<llvm::Value*> cmpz;
  std::fill(fallT.begin(), fallT.end(), nullptr);

  llvm::Value *vNPC = regTbl.gprTbl[r.r.rs1];
  int32_t tgt = r.jj.imm11_0;
  tgt |= ((inst>>31)&1) ? 0xfffff000 : 0x0;
  vNPC = cfg->myIRBuilder->CreateAdd(vNPC, llvm::ConstantInt::get(iType32,tgt));
  vNPC = cfg->myIRBuilder->CreateAnd(vNPC, llvm::ConstantInt::get(iType32,(~1U)));
  
  for(cfgBasicBlock* next : cBB->succs) {
      uint32_t nAddr = next->getEntryAddr();
      llvm::Value *vAddr = llvm::ConstantInt::get(iType32,nAddr);
      llvm::Value *vCmp = cfg->myIRBuilder->CreateICmpEQ(vNPC, vAddr);
      cmpz.push_back(vCmp);
    }

  size_t p = 0;
  fallT[p++] = llvm::BasicBlock::Create(cxt,"ft",cfg->blockFunction);
  cfg->myIRBuilder->CreateBr(fallT[0]);
  cfg->myIRBuilder->SetInsertPoint(fallT[0]);
  for(cfgBasicBlock* next : cBB->succs) {
      size_t pp = p-1;
      fallT[p++] = llvm::BasicBlock::Create(cxt,"ft",cfg->blockFunction);
      cfg->myIRBuilder->CreateCondBr(cmpz[pp], next->lBB, fallT[p-1]);
      cBB->jrMap[next->lBB] = fallT[pp];
      cfg->myIRBuilder->SetInsertPoint(fallT[p-1]);
    }
  llvm::BasicBlock *abortBlock = cfg->generateAbortBasicBlock(vNPC, regTbl, cBB, nullptr);
  cfg->myIRBuilder->CreateBr(abortBlock);

  cBB->hasTermBranchOrJump = true;

  return true;
}

bool insn_jalr::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);

  regTbl.gprTbl[r.jj.rd] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cxt),(addr+4));

  std::vector<llvm::BasicBlock*> fallT(cBB->succs.size() + 1);
  std::vector<llvm::Value*> cmpz;
  std::fill(fallT.begin(), fallT.end(), nullptr);

  llvm::Value *vNPC = regTbl.gprTbl[r.jj.rs1];

  for(cfgBasicBlock *next : cBB->succs) {
    uint32_t nAddr = next->getEntryAddr();
    llvm::Value *vAddr = llvm::ConstantInt::get(iType32,nAddr);
    llvm::Value *vCmp = cfg->myIRBuilder->CreateICmpEQ(vNPC, vAddr);
    cmpz.push_back(vCmp);
  }
  

  size_t p = 0;
  fallT[p++] = llvm::BasicBlock::Create(cxt,"ft",cfg->blockFunction);
  cfg->myIRBuilder->CreateBr(fallT[0]);
  cfg->myIRBuilder->SetInsertPoint(fallT[0]);
  for(cfgBasicBlock* next : cBB->succs) {
      size_t pp = p-1;
      fallT[p++] = llvm::BasicBlock::Create(cxt,"ft",cfg->blockFunction);
      cfg->myIRBuilder->CreateCondBr(cmpz[pp], next->lBB, fallT[p-1]);
      cBB->jrMap[next->lBB] = fallT[pp];
      cfg->myIRBuilder->SetInsertPoint(fallT[p-1]);
    }
  llvm::BasicBlock *abortBlock = cfg->generateAbortBasicBlock(vNPC, regTbl, cBB, nullptr);
  cfg->myIRBuilder->CreateBr(abortBlock);

  cBB->hasTermBranchOrJump = true;

  return true;
}


