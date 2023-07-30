#include <cstdio>
#include <cstdlib>
#include "disassemble.hh"
#include "riscvInstruction.hh"
#include "regionCFG.hh"
#include "helper.hh"
#include "globals.hh"

#include "mips.hh"

using namespace mips;

typedef llvm::Value lv_t;

#define TC ((fmt==FMT_D?fprUseEnum::doublePrec : fprUseEnum::singlePrec))

std::ostream &operator<<(std::ostream &out, const Insn &ins) {
  out << "0x" << std::hex << ins.addr << std::dec 
      << " : " << getAsmString(ins.inst, ins.addr) 
      << std::endl;
  return out;
}

class iTypeLoadInsn : public iTypeInsn {
public:
  iTypeLoadInsn(uint32_t inst, uint32_t addr) : iTypeInsn(inst, addr) {}
};

class iTypeStoreInsn : public iTypeInsn {
public:
  iTypeStoreInsn(uint32_t inst, uint32_t addr) :
    iTypeInsn(inst, addr,insnDefType::no_dest) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;
};


class simpleRType : public rTypeInsn {
protected:
  enum class rtype {
    sll, srl, sra, srlv, srav,
    addu, add, subu, sub, and_, 
    or_, xor_, nor_, slt, sltu,
    teq, sllv, movn, movz};
  rtype r_type;
public:
  simpleRType(uint32_t inst, uint32_t addr, rtype r_type) :
    rTypeInsn(inst,addr), r_type(r_type) {}
  bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};

class simpleIType : public iTypeInsn {
protected:
  enum class itype {addiu,andi,lui,ori,xori};
  itype i_type;
public:
  simpleIType(uint32_t inst, uint32_t addr, itype i_type) :
    iTypeInsn(inst,addr), i_type(i_type) {}
  bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};

/* R type */
class insn_sll : public simpleRType {
public:
  insn_sll(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::sll) {}
};

class insn_srl : public simpleRType {
 public:
 insn_srl(uint32_t inst, uint32_t addr) :
   simpleRType(inst, addr, simpleRType::rtype::srl) {}
};

class insn_sra : public simpleRType {
 public:
  insn_sra(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::sra) {}
};

class insn_sllv : public simpleRType {
public:
  insn_sllv(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::sllv) {}
};

class insn_srlv : public simpleRType {
 public:
  insn_srlv(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::srlv) {}
};

class insn_srav : public simpleRType {
public:
  insn_srav(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::srav) {}
};

class insn_syscall : public rTypeInsn {
 public:
  insn_syscall(uint32_t inst, uint32_t addr) :
    rTypeInsn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {}
};

class insn_break : public rTypeInsn {
public:
  insn_break(uint32_t inst, uint32_t addr) :
    rTypeInsn(inst, addr) {} 
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {}
};

class insn_sync : public rTypeInsn {
public:
  insn_sync(uint32_t inst, uint32_t addr) :
    rTypeInsn(inst, addr) {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
  void recUses(cfgBasicBlock *cBB) override {}
};



class insn_add : public rTypeInsn {
public:
  insn_add(uint32_t inst, uint32_t addr) : rTypeInsn(inst, addr) {}
};

class insn_addu : public simpleRType {
public:
  insn_addu(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::addu) {}
};

class insn_sub : public rTypeInsn {
public:
  insn_sub(uint32_t inst, uint32_t addr) : rTypeInsn(inst, addr) {}
};

class insn_subu : public simpleRType {
public:
  insn_subu(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::subu) {}
};

class insn_and : public simpleRType {
public:
  insn_and(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::and_) {}
};

class insn_or : public simpleRType {
public:
  insn_or(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::or_) {}
};

class insn_xor : public simpleRType {
public:
  insn_xor(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::xor_) {}
};

class insn_nor : public simpleRType {
public:
  insn_nor(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::nor_) {}
};

class insn_slt : public simpleRType {
public:
  insn_slt(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr,simpleRType::rtype::slt) {}
};

class insn_sltu : public simpleRType {
public:
  insn_sltu(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr,simpleRType::rtype::sltu) {}
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

class insn_addi : public iTypeInsn {
 public:
 insn_addi(uint32_t inst, uint32_t addr) : iTypeInsn(inst, addr) {}
 
};

class insn_addiu : public simpleIType {
 public:
  insn_addiu(uint32_t inst, uint32_t addr) :
    simpleIType(inst, addr, simpleIType::itype::addiu) {}
};

class insn_slti : public iTypeInsn {
 public:
 insn_slti(uint32_t inst, uint32_t addr) : iTypeInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
 
};

class insn_sltiu : public iTypeInsn {
 public:
 insn_sltiu(uint32_t inst, uint32_t addr) : iTypeInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
 
};

class insn_andi : public simpleIType {
public:
  insn_andi(uint32_t inst, uint32_t addr) :
    simpleIType(inst, addr,simpleIType::itype::andi) {}
};

class insn_ori : public simpleIType {
 public:
  insn_ori(uint32_t inst, uint32_t addr) :
    simpleIType(inst, addr,simpleIType::itype::ori) {}
};

class insn_xori : public simpleIType {
 public:
  insn_xori(uint32_t inst, uint32_t addr) :
    simpleIType(inst, addr,simpleIType::itype::xori) {}
};

class insn_lui : public simpleIType {
 public:
 insn_lui(uint32_t inst, uint32_t addr) :
   simpleIType(inst, addr, simpleIType::itype::lui) {}
};


class insn_lb : public iTypeLoadInsn {
 public:
 insn_lb(uint32_t inst, uint32_t addr) : iTypeLoadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
 
};

class insn_lh : public iTypeLoadInsn {
 public:
 insn_lh(uint32_t inst, uint32_t addr) : iTypeLoadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
 
};

class insn_lw : public iTypeLoadInsn {
 public:
 insn_lw(uint32_t inst, uint32_t addr) : iTypeLoadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
 
};

class insn_lbu : public iTypeLoadInsn {
 public:
 insn_lbu(uint32_t inst, uint32_t addr) : iTypeLoadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
 
};

class insn_lhu : public iTypeLoadInsn {
 public:
 insn_lhu(uint32_t inst, uint32_t addr) : iTypeLoadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
 
};


class insn_sb : public iTypeStoreInsn {
 public:
 insn_sb(uint32_t inst, uint32_t addr) : iTypeStoreInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
 
};

class insn_sh : public iTypeStoreInsn {
 public:
 insn_sh(uint32_t inst, uint32_t addr) : iTypeStoreInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
 
};

class insn_sw : public iTypeStoreInsn {
 public:
  insn_sw(uint32_t inst, uint32_t addr) : iTypeStoreInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) override;
};



std::string Insn::getString() const {
  std::stringstream ss;
  ss << *this;
  return ss.str();
}

bool Insn::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl){
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

llvm::Value *Insn::byteSwap(llvm::Value *v) {
  if(globals::isMipsEL) {
    return v;
  }
  else {
    std::vector<llvm::Type*> typeVec;
    typeVec.push_back(v->getType());
    llvm::ArrayRef<llvm::Type*> typeArrRef(typeVec);
    auto vswapIntr = llvm::Intrinsic::getDeclaration(cfg->myModule, 
							     llvm::Intrinsic::bswap, 
							     typeArrRef);
    llvm::Value *vswapCall = cfg->myIRBuilder->CreateCall(vswapIntr, v);
    return vswapCall;
  }
}

void Insn::emitPrintPC() {
#if 0
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  llvm::Value *vAddr = llvm::ConstantInt::get(iType32,addr);
  std::vector<llvm::Value*> argVector;
  argVector.push_back(vAddr);
  llvm::ArrayRef<llvm::Value*> cArgs(argVector);
  cfg->myIRBuilder->CreateCall(cfg->builtinFuncts["print_pc"],cArgs);
#endif
}

void Insn::saveInstAddress() {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType64 = llvm::Type::getInt64Ty(cxt);
  llvm::Value *vZ = llvm::ConstantInt::get(iType64,0);
  llvm::Value *vAddr = llvm::ConstantInt::get(iType64,(uint64_t)addr);
  llvm::Value *vG = cfg->myIRBuilder->CreateGEP(cfg->blockArgMap["icnt"], vZ);
  cfg->myIRBuilder->CreateStore(vAddr, vG);
}

void Insn::codeGen(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  generateIR(cBB, regTbl);
}


/* r-type */
void rTypeInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  //printf("rTYPE : %s defines %s\n", getAsmString(inst,addr).c_str(), 
  //getGPRName(rd, false).c_str());
  if(rd != 0)
    cfg->gprDefinitionBlocks[rd].insert(cBB);
}
void rTypeInsn::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[rs]=true;
  cBB->gprRead[rt]=true;
}



void insn_jalr::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[R_ra].insert(cBB);
}
void insn_jalr::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[rs]=true;
}
void insn_jr::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[rs]=true;
}


bool iBranchTypeInsn::handleBranch(cfgBasicBlock *cBB,
				   llvmRegTables& regTbl,
				   llvm::CmpInst::Predicate branchPred,
				   llvm::Value *vRT, llvm::Value *vRS) {
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmp(branchPred, vRS, vRT);


  llvm::BasicBlock *tBB = cBB->getSuccLLVMBasicBlock(tAddr);
  llvm::BasicBlock *ntBB = cBB->getSuccLLVMBasicBlock(ntAddr);
  if(tAddr != ntAddr) {
    llvm::BasicBlock *t0 = cfg->generateAbortBasicBlock(tAddr, regTbl,cBB,tBB,addr);
    llvm::BasicBlock *t1 = ntBB = cfg->generateAbortBasicBlock(ntAddr,regTbl,cBB,ntBB,addr);
    bool tIsAbort = not(tBB == t0);
    tBB = t0;
    ntBB = t1;
    llvm::Instruction *TI = cfg->myIRBuilder->CreateCondBr(vCMP, tBB, ntBB);
#if 1
    llvm::MDBuilder MDB(globalContext);
    llvm::MDNode *Node = nullptr;
    if(tIsAbort)
      Node  = MDB.createBranchWeights(5,95);
    else
      Node  = MDB.createBranchWeights(95,5);
    TI->setMetadata(llvm::LLVMContext::MD_prof, Node);
#endif
  }
  else {
    tBB = cfg->generateAbortBasicBlock(tAddr, regTbl,cBB,tBB,addr);
    cfg->myIRBuilder->CreateBr(tBB);
  }
  
  cBB->hasTermBranchOrJump = true;

  return true;

}



void iTypeInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[rt].insert(cBB);
}
void iTypeInsn::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[rs]=true;
}
void iBranchTypeInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  /* don't update anything.. */
}
void iBranchTypeInsn::recUses(cfgBasicBlock *cBB) {
  /* branches read both operands */
  cBB->gprRead[r.b.rs1]=true;
  cBB->gprRead[r.b.rs2]=true;
}
void iTypeStoreInsn::recUses(cfgBasicBlock *cBB) {
  /* stores read both operands */
  cBB->gprRead[r.b.rs1]=true;
  cBB->gprRead[r.r.rs2]=true;
}
void iTypeStoreInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  /* don't update anything.. */
}
void insn_jal::recDefines(cfgBasicBlock *cBB, regionCFG *cfg)  {
  uint32_t rd = (inst>>7) & 31;
  assert(rd != 0);
  cfg->gprDefinitionBlocks[rd].insert(cBB);
}


Insn* getInsn(uint32_t inst, uint32_t addr){
  uint32_t opcode = inst & 127;
  riscv_t m(inst);
  std::cout << "opcode = "
	    << std::hex
	    << opcode
	    << std::dec
	    << "\n";
  switch(opcode)
    {
    case 0x3:  /* loads */
    case 0xf:  /* fence - there's a bunch of 'em */
      break;
    case 0x13: /* reg + imm insns */
      switch(m.i.sel)
	{
	case 0: /* addi */
	  return new insn_addi(inst, addr);
#if 0 
	case 1: /* slli */
	  return new insn_slli(inst, addr);
#endif
	case 2: /* slti */
	  return new insn_slti(inst, addr);
	case 3: /* sltiu */
	  return new insn_sltiu(inst, addr);
	case 4: /* xori */
	  return new insn_xori(inst, addr);
#if 0
	case 5: { /* srli & srai */
	  uint32_t sel =  (inst >> 25) & 127;	    
	  if(sel == 0) { /* srli */
	    return new insn_srli(inst, addr);
	  }
	  else if(sel == 32) { /* srai */
	    return new insn_srai(inst, addr);
	  }
	  else {
	    assert(0);
	  }
	  break;
	}
#endif
	case 6: /* ori */
	  return new insn_ori(inst, addr);
	case 7: /* andi */
	  return new insn_andi(inst, addr);
	default:
	  return nullptr;
	}
      break;
    case 0x23: {/* stores */
      switch(m.s.sel) {
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
    case 0x17: /* auipc */
    case 0x67: /* jalr */
    case 0x6f: /* jal */
    case 0x33:  /* reg + reg insns */
      return nullptr;
    case 0x63: /* branches */
      std::cout << m.b.sel << "\n";
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




bool simpleIType::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  using namespace llvm;
  LLVMContext &cxt = *(cfg->Context);
  Type *iType32 = Type::getInt32Ty(cxt);
  Value *vRT = nullptr;
  switch(i_type)
    {
    case itype::addiu:
    vRT = cfg->myIRBuilder->CreateAdd(regTbl.gprTbl[rs],
				      ConstantInt::get(iType32,simm));
    break;
    case itype::andi:
      vRT = cfg->myIRBuilder->CreateAnd(regTbl.gprTbl[rs],
					ConstantInt::get(iType32,uimm));
      break;
    case itype::lui:
      vRT = ConstantInt::get(iType32,uimm<<16);
      break;
    case itype::ori:
      vRT = cfg->myIRBuilder->CreateOr(regTbl.gprTbl[rs],
				       ConstantInt::get(iType32,uimm));
      break;
    case itype::xori:
      vRT = cfg->myIRBuilder->CreateXor(regTbl.gprTbl[rs],
					ConstantInt::get(iType32,uimm));
      break;
    default:
      assert(false);
    }
  regTbl.gprTbl[rt] = vRT;
  return false;
}

bool simpleRType::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  using namespace llvm;
  LLVMContext &cxt = *(cfg->Context);
  Value *vRD = nullptr;
  uint32_t sa = (inst >> 6) & 31;
  Type *iType32 = Type::getInt32Ty(cxt);
  Value* vSA = ConstantInt::get(iType32,sa);
  Value* vM5 = ConstantInt::get(iType32,0x1f);
  Value *vZ = ConstantInt::get(iType32,0);
  Value *vO = ConstantInt::get(iType32,1);
  
  switch(r_type)
    {
    case rtype::addu:
      vRD = cfg->myIRBuilder->CreateAdd(regTbl.gprTbl[rs], regTbl.gprTbl[rt]);
      break;
    case rtype::subu:
      vRD = cfg->myIRBuilder->CreateSub(regTbl.gprTbl[rs], regTbl.gprTbl[rt]);
      break;
    case rtype::sll:
      vRD = cfg->myIRBuilder->CreateShl(regTbl.gprTbl[rt], vSA);
      break;
    case rtype::srl:
      vRD = cfg->myIRBuilder->CreateLShr(regTbl.gprTbl[rt], vSA);
      break;
    case rtype::sra:
      vRD = cfg->myIRBuilder->CreateAShr(regTbl.gprTbl[rt], vSA);
      break;
    case rtype::sllv:
      vRD = cfg->myIRBuilder->CreateAnd(regTbl.gprTbl[rs], vM5);
      vRD = cfg->myIRBuilder->CreateShl(regTbl.gprTbl[rt], vRD);
      break;
    case rtype::srlv:
      vRD = cfg->myIRBuilder->CreateAnd(regTbl.gprTbl[rs], vM5);
      vRD = cfg->myIRBuilder->CreateLShr(regTbl.gprTbl[rt], vRD);
      break;
    case rtype::srav:
      vRD = cfg->myIRBuilder->CreateAnd(regTbl.gprTbl[rs], vM5);
      vRD = cfg->myIRBuilder->CreateAShr(regTbl.gprTbl[rt], vRD);
      break;
    case rtype::and_:
      vRD = cfg->myIRBuilder->CreateAnd(regTbl.gprTbl[rs],regTbl.gprTbl[rt]);
      break;
    case rtype::or_:
      vRD = cfg->myIRBuilder->CreateOr(regTbl.gprTbl[rs],regTbl.gprTbl[rt]);
      break;
    case rtype::xor_:
      vRD = cfg->myIRBuilder->CreateXor(regTbl.gprTbl[rs],regTbl.gprTbl[rt]);
      break;
    case rtype::nor_:
      vRD = cfg->myIRBuilder->CreateOr(regTbl.gprTbl[rs],regTbl.gprTbl[rt]);
      vRD = cfg->myIRBuilder->CreateNot(vRD);
      break;
    case rtype::slt:
      vRD = cfg->myIRBuilder->CreateICmpSLT(regTbl.gprTbl[rs],regTbl.gprTbl[rt]);
      vRD = cfg->myIRBuilder->CreateSelect(vRD, vO, vZ);
      break;
    case rtype::sltu:
      vRD = cfg->myIRBuilder->CreateICmpULT(regTbl.gprTbl[rs],regTbl.gprTbl[rt]);
      vRD = cfg->myIRBuilder->CreateSelect(vRD, vO, vZ);
      break;
    default:
      assert(false);
    }
  regTbl.gprTbl[rd] = vRD;
  return false;
}


bool insn_sb::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*(cfg->Context)),simm);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vRT = regTbl.gprTbl[rt];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA = cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(*(cfg->Context)));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->CreateGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP,llvm::Type::getInt8PtrTy(*(cfg->Context)));
  llvm::Value *vTrunc = cfg->myIRBuilder->CreateTrunc(vRT, llvm::Type::getInt8Ty(*(cfg->Context)));
  cfg->myIRBuilder->CreateStore(vTrunc, vPtr);
  return false;
}

bool insn_sh::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cxt),simm);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vRT = regTbl.gprTbl[rt];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA = cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(cxt));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->CreateGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt16PtrTy(cxt));
  llvm::Value *vTrunc = cfg->myIRBuilder->CreateTrunc(vRT, llvm::Type::getInt16Ty(cxt));
  llvm::Value *vSwap = byteSwap(vTrunc);
  cfg->myIRBuilder->CreateStore(vSwap, vPtr);
  return false;
}



bool insn_sw::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cxt),simm);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vRT = regTbl.gprTbl[rt];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA = cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(cxt));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->CreateGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt32PtrTy(cxt));
  llvm::Value *vSwap = byteSwap(vRT);
  cfg->myIRBuilder->CreateStore(vSwap, vPtr);
  return false;
}


bool insn_lbu::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*(cfg->Context)),simm);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA =  cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(*(cfg->Context)));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->CreateGEP(vMem, vZEA);
  std::string loadName = "lbu_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vLoad = cfg->myIRBuilder->CreateLoad(vGEP,loadName);
  llvm::Value *vSext = cfg->myIRBuilder->CreateZExt(vLoad, llvm::Type::getInt32Ty(*(cfg->Context)));
  regTbl.gprTbl[rt] = vSext;
  return false;
}

bool insn_lb::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  llvm::Type *iType64 = llvm::Type::getInt64Ty(cxt);
  llvm::Value *vIMM = llvm::ConstantInt::get(iType32,simm);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA = cfg->myIRBuilder->CreateZExt(vEA, iType64);
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->CreateGEP(vMem, vZEA);
  std::string loadName = "lb_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vLoad = cfg->myIRBuilder->CreateLoad(vGEP,loadName);
  regTbl.gprTbl[rt] = cfg->myIRBuilder->CreateSExt(vLoad,iType32);
  return false;
}

bool insn_lh::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*(cfg->Context)),simm);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA =  cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(*(cfg->Context)));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->CreateGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt16PtrTy(*(cfg->Context)));
  std::string loadName = "lh_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vLoad = cfg->myIRBuilder->CreateLoad(vPtr,loadName);
  llvm::Value *vSwap = byteSwap(vLoad);
  llvm::Value *vSext = cfg->myIRBuilder->CreateSExt(vSwap,  
						    llvm::Type::getInt32Ty(*(cfg->Context)));
  regTbl.gprTbl[rt] = vSext;
  return false;
}

bool insn_lhu::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  llvm::Type *iType64 = llvm::Type::getInt64Ty(cxt);
  llvm::Value *vIMM = llvm::ConstantInt::get(iType32, simm);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA =  cfg->myIRBuilder->CreateZExt(vEA, iType64);
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->CreateGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt16PtrTy(cxt));
  std::string loadName = "lhu_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vLoad = cfg->myIRBuilder->CreateLoad(vPtr,loadName);
  llvm::Value *vSext = cfg->myIRBuilder->CreateZExt(byteSwap(vLoad), iType32);
  regTbl.gprTbl[rt] = vSext;
  return false;
}

bool insn_lw::generateIR(cfgBasicBlock *cBB, llvmRegTables& regTbl) {
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*(cfg->Context)),simm);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA =  cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(*(cfg->Context)));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->CreateGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt32PtrTy(*(cfg->Context)));
  std::string loadName = "lw_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vLoad = cfg->myIRBuilder->CreateLoad(vPtr,loadName);
  regTbl.gprTbl[rt] = byteSwap(vLoad);
  return false;
}




bool insn_slti::generateIR(cfgBasicBlock *cBB,   llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  llvm::Value *vImm = llvm::ConstantInt::get(iType32,simm);
  llvm::Value *vZ = llvm::ConstantInt::get(iType32,0);
  llvm::Value *vO = llvm::ConstantInt::get(iType32,1);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmpSLT(vRS, vImm);
  regTbl.gprTbl[rt] = cfg->myIRBuilder->CreateSelect(vCMP, vO, vZ);
  return false;
}

bool insn_sltiu::generateIR(cfgBasicBlock *cBB,   llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  llvm::Value *vImm = llvm::ConstantInt::get(iType32,simm);
  llvm::Value *vZ = llvm::ConstantInt::get(iType32,0);
  llvm::Value *vO = llvm::ConstantInt::get(iType32,1);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmpULT(vRS, vImm);
  regTbl.gprTbl[rt] = cfg->myIRBuilder->CreateSelect(vCMP, vO, vZ);
  return false;
}

bool insn_bltu::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vRT = regTbl.gprTbl[r.b.rs1];
  llvm::Value *vRS = regTbl.gprTbl[r.r.rs2];
  return handleBranch(cBB, regTbl,llvm::CmpInst::ICMP_SLT, vRT, vRS);
}

bool insn_blt::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vRT = regTbl.gprTbl[r.b.rs1];
  llvm::Value *vRS = regTbl.gprTbl[r.b.rs2];
  return handleBranch(cBB,regTbl,llvm::CmpInst::ICMP_SLT, vRT, vRS);
}

bool insn_bne::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vRT = regTbl.gprTbl[r.b.rs1];
  llvm::Value *vRS = regTbl.gprTbl[r.b.rs2];
  return handleBranch(cBB,regTbl,llvm::CmpInst::ICMP_NE,vRT,vRS);
}


bool insn_beq::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vRT = regTbl.gprTbl[r.b.rs1];
  llvm::Value *vRS = regTbl.gprTbl[r.b.rs2];
  return handleBranch(cBB,regTbl,llvm::CmpInst::ICMP_EQ, vRT, vRS);
}


bool insn_bge::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vRT = regTbl.gprTbl[r.b.rs1];
  llvm::Value *vRS = regTbl.gprTbl[r.b.rs2];
  return handleBranch(cBB,regTbl,llvm::CmpInst::ICMP_NE,vRT,vRS);
}

bool insn_bgeu::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
  llvm::Value *vRT = regTbl.gprTbl[r.b.rs1];
  llvm::Value *vRS = regTbl.gprTbl[r.b.rs2];
  return handleBranch(cBB,regTbl,llvm::CmpInst::ICMP_NE,vRT,vRS);
}


bool insn_j::generateIR(cfgBasicBlock *cBB,  llvmRegTables& regTbl) {
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
  regTbl.gprTbl[31] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cxt),(addr+8));
  cfgBasicBlock *nBB = *(cBB->succs.begin());
#if 0
  if(cBB->succs.size() != 1) {
    printf("not 1 succ in a jal (%zu succs)\n", cBB->succs.size());
    exit(-1);
  }
#endif
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

  llvm::Value *vNPC = regTbl.gprTbl[rs];

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

  regTbl.gprTbl[31] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cxt),(addr+8));

  std::vector<llvm::BasicBlock*> fallT(cBB->succs.size() + 1);
  std::vector<llvm::Value*> cmpz;
  std::fill(fallT.begin(), fallT.end(), nullptr);

  llvm::Value *vNPC = regTbl.gprTbl[rs];

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


