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
  void updateGPRConstants(std::vector<regState> &gprConstState) override {
    gprConstState[rt].e = variant;
    gprConstState[rt].v = ~0;
  }
};

class iBranchLikelyTypeInsn : public iBranchTypeInsn {
public:
  iBranchLikelyTypeInsn(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst,addr) {}
  bool handleBranch(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl,
		    llvm::CmpInst::Predicate branchPred,
		    llvm::Value *vRT, llvm::Value *vRS) override ;
};

class iTypeStoreInsn : public iTypeInsn {
public:
  iTypeStoreInsn(uint32_t inst, uint32_t addr) :
    iTypeInsn(inst, addr,insnDefType::no_dest) {}
  void updateGPRConstants(std::vector<regState> &gprConstState) override {}
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;
};



class specialType2Insn : public Insn {
public:
  specialType2Insn(uint32_t inst, uint32_t addr):
    Insn(inst, addr) {}
};

class specialType3Insn : public Insn {
 public:
 specialType3Insn(uint32_t inst, uint32_t addr):
   Insn(inst, addr) {}
};

class coprocType0Insn : public Insn {
 public:
 coprocType0Insn(uint32_t inst, uint32_t addr):
   Insn(inst, addr) {}
};



class coprocType1xInsn : public Insn {
protected:
  mips_t mi;
public:
  coprocType1xInsn(uint32_t inst, uint32_t addr) : Insn(inst, addr), mi(inst) {}
  bool isFloatingPoint() const override {
    return true;
  }
  opPrecType getPrecType() const override;
  bool canCompile() const override;
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;
};


class coprocType2Insn : public Insn {
 public:
  coprocType2Insn(uint32_t inst, uint32_t addr) :
    Insn(inst, addr) {}
};


class insn_mul : public specialType2Insn {
protected:
  uint32_t rs,rt,rd;
public:
  insn_mul(uint32_t inst, uint32_t addr) :
    specialType2Insn(inst, addr), rs((inst >> 21) & 31),
    rt((inst >> 16) & 31), rd((inst >> 11) & 31) {}
  void updateGPRConstants(std::vector<regState> &gprConstState) override ; 
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;
};

class insn_msub : public specialType2Insn {
protected:
  uint32_t rs,rt;
public:
  insn_msub(uint32_t inst, uint32_t addr) :
    specialType2Insn(inst, addr), rs((inst >> 21) & 31),
    rt((inst >> 16) & 31) {};
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;
};


class insn_clz : public specialType2Insn {
protected:
  uint32_t rs,rt,rd;
public:
  insn_clz(uint32_t inst, uint32_t addr) :
    specialType2Insn(inst, addr),rs((inst >> 21) & 31),
    rt((inst >> 16) & 31), rd((inst >> 11) & 31) {}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;
};
 
/* Special 3 */
class insn_seb : public specialType3Insn {
protected:
  uint32_t rt,rd;
public:
  insn_seb(uint32_t inst, uint32_t addr) :
    specialType3Insn(inst, addr),rt((inst >> 16) & 31), rd((inst >> 11) & 31){}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;
};

class insn_seh : public specialType3Insn {
 protected:
  uint32_t rt,rd;
public:
  insn_seh(uint32_t inst, uint32_t addr) :
    specialType3Insn(inst, addr), rt((inst >> 16) & 31),
    rd((inst >> 11) & 31){}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;
  void updateGPRConstants(std::vector<regState> &gprConstState) override {
    gprConstState[rd].e = variant;
  }
};

class insn_ext : public specialType3Insn {
protected:
  uint32_t rt,rs;
public:
  insn_ext(uint32_t inst, uint32_t addr) :
    specialType3Insn(inst, addr),
    rt((inst >> 16) & 31),
    rs((inst >> 21) & 31) {}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;
  void updateGPRConstants(std::vector<regState> &gprConstState) override {
    gprConstState[rt].e = variant;
  }
};

class insn_ins : public specialType3Insn {
protected:
  uint32_t rs,rt,lsb,msb,size,mask,cmask;
public:
  insn_ins(uint32_t inst, uint32_t addr) : specialType3Insn(inst, addr) {
    rt = (inst >> 16) & 31;
    rs = (inst >> 21) & 31;
    lsb = (inst >> 6) & 31;
    msb = ((inst >> 11) & 31);
    size = msb-lsb+1;
    mask = (1U<<size) -1;
    cmask = ~(mask << lsb);
  };
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
  void recUses(cfgBasicBlock *cBB) override;
  void updateGPRConstants(std::vector<regState> &gprConstState) override {
    gprConstState[rt].e = variant;
  }
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
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
  uint32_t doOp(uint32_t a, uint32_t b) override;
};

class simpleIType : public iTypeInsn {
protected:
  enum class itype {addiu,andi,lui,ori,xori};
  itype i_type;
public:
  simpleIType(uint32_t inst, uint32_t addr, itype i_type) :
    iTypeInsn(inst,addr), i_type(i_type) {}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

/* R type */
class insn_sll : public simpleRType {
public:
  insn_sll(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::sll) {}
  void updateGPRConstants(std::vector<regState> &gprConstState) override ;
};

class insn_movci : public rTypeInsn {
 public:
 insn_movci(uint32_t inst, uint32_t addr) : rTypeInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override ;
 void recUses(cfgBasicBlock *cBB) override;
 void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
};

class insn_srl : public simpleRType {
 public:
 insn_srl(uint32_t inst, uint32_t addr) :
   simpleRType(inst, addr, simpleRType::rtype::srl) {}
  void updateGPRConstants(std::vector<regState> &gprConstState) override;
};

class insn_sra : public simpleRType {
 public:
  insn_sra(uint32_t inst, uint32_t addr) :
    simpleRType(inst, addr, simpleRType::rtype::sra) {}
  void updateGPRConstants(std::vector<regState> &gprConstState) override;
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

class insn_monitor : public rTypeInsn {
private:
  uint32_t reason;
 public:
  insn_monitor(uint32_t inst, uint32_t addr) :
    rTypeInsn(inst, addr), reason(((inst >> RSVD_INSTRUCTION_ARG_SHIFT) &
				   RSVD_INSTRUCTION_ARG_MASK)) {}
  bool canCompile() const override {
    
    return false;
  }
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {};
};

class insn_movn : public rTypeInsn {
public:
  insn_movn(uint32_t inst, uint32_t addr) : rTypeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_movz : public rTypeInsn {
public:
  insn_movz(uint32_t inst, uint32_t addr) :
   rTypeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
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

class insn_tge : public rTypeInsn {
public:
  insn_tge(uint32_t inst, uint32_t addr) : rTypeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
};

class insn_teq : public rTypeInsn {
 public:
  insn_teq(uint32_t inst, uint32_t addr) : rTypeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
  void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
};

/* iType */
class insn_beq : public iBranchTypeInsn {
 public:
 insn_beq(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_bne : public iBranchTypeInsn {
 public:
 insn_bne(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};


class insn_blt : public iBranchTypeInsn {
 public:
  insn_blt(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_bge : public iBranchTypeInsn {
public:
  insn_bge(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_bltu : public iBranchTypeInsn {
 public:
  insn_bltu(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_bgeu : public iBranchTypeInsn {
public:
  insn_bgeu(uint32_t inst, uint32_t addr) : iBranchTypeInsn(inst, addr) {}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_addi : public iTypeInsn {
 public:
 insn_addi(uint32_t inst, uint32_t addr) : iTypeInsn(inst, addr) {}
 
};

class insn_addiu : public simpleIType {
 public:
  insn_addiu(uint32_t inst, uint32_t addr) :
    simpleIType(inst, addr, simpleIType::itype::addiu) {}
 void updateGPRConstants(std::vector<regState> &gprConstState) override;
};

class insn_slti : public iTypeInsn {
 public:
 insn_slti(uint32_t inst, uint32_t addr) : iTypeInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
 
};

class insn_sltiu : public iTypeInsn {
 public:
 insn_sltiu(uint32_t inst, uint32_t addr) : iTypeInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
 
};

class insn_andi : public simpleIType {
public:
  insn_andi(uint32_t inst, uint32_t addr) :
    simpleIType(inst, addr,simpleIType::itype::andi) {}
  void updateGPRConstants(std::vector<regState> &gprConstState) override;
};

class insn_ori : public simpleIType {
 public:
  insn_ori(uint32_t inst, uint32_t addr) :
    simpleIType(inst, addr,simpleIType::itype::ori) {}
 void updateGPRConstants(std::vector<regState> &gprConstState) override;
};

class insn_xori : public simpleIType {
 public:
  insn_xori(uint32_t inst, uint32_t addr) :
    simpleIType(inst, addr,simpleIType::itype::xori) {}
  void updateGPRConstants(std::vector<regState> &gprConstState) override;
};

class insn_lui : public simpleIType {
 public:
 insn_lui(uint32_t inst, uint32_t addr) :
   simpleIType(inst, addr, simpleIType::itype::lui) {}
 void updateGPRConstants(std::vector<regState> &gprConstState) override;
};


class insn_lb : public iTypeLoadInsn {
 public:
 insn_lb(uint32_t inst, uint32_t addr) : iTypeLoadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
 
};

class insn_lh : public iTypeLoadInsn {
 public:
 insn_lh(uint32_t inst, uint32_t addr) : iTypeLoadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
 
};

class insn_lw : public iTypeLoadInsn {
 public:
 insn_lw(uint32_t inst, uint32_t addr) : iTypeLoadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
 
};

class insn_lbu : public iTypeLoadInsn {
 public:
 insn_lbu(uint32_t inst, uint32_t addr) : iTypeLoadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
 
};

class insn_lhu : public iTypeLoadInsn {
 public:
 insn_lhu(uint32_t inst, uint32_t addr) : iTypeLoadInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
 
};


class insn_sb : public iTypeStoreInsn {
 public:
 insn_sb(uint32_t inst, uint32_t addr) : iTypeStoreInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
 
};

class insn_sh : public iTypeStoreInsn {
 public:
 insn_sh(uint32_t inst, uint32_t addr) : iTypeStoreInsn(inst, addr) {}
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
 
};

class insn_sw : public iTypeStoreInsn {
 public:
 insn_sw(uint32_t inst, uint32_t addr) : iTypeStoreInsn(inst, addr) {}
 
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_lwc1 : public iTypeInsn {
protected:
 uint32_t ft;
 public:
 insn_lwc1(uint32_t inst, uint32_t addr) : iTypeInsn(inst, addr), ft((inst >> 16) & 31) {}
 bool isFloatingPoint() const override {
  return true;
 }
 opPrecType getPrecType() const override {
  return singleprec;
 }
 bool canCompile() const override;
 
 void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_ldc1 : public iTypeInsn {
protected:
 uint32_t ft;
 public:
 insn_ldc1(uint32_t inst, uint32_t addr) : iTypeInsn(inst, addr), ft((inst >> 16) & 31) {}
 bool isFloatingPoint() const override {
  return true;
 }
 opPrecType getPrecType() const override {
  return doubleprec;
 }
 bool canCompile() const override;
 
 void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_swc1 : public iTypeInsn {
protected:
 uint32_t ft;
public:
 insn_swc1(uint32_t inst, uint32_t addr) : iTypeInsn(inst, addr), ft((inst >> 16) & 31) {
 }
 bool isFloatingPoint() const override {
  return true;
 }
 opPrecType getPrecType() const override {
  return singleprec;
 }
 bool canCompile() const override ;
 
 void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
 void recUses(cfgBasicBlock *cBB) override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_sdc1 : public iTypeInsn {
protected:
 uint32_t ft;
public:
 insn_sdc1(uint32_t inst, uint32_t addr) :
  iTypeInsn(inst, addr), ft((inst >> 16) & 31) {
 }
 bool isFloatingPoint() const override {
  return true;
 }
 opPrecType getPrecType() const override {
  return doubleprec;
 }
 bool canCompile() const override;
 
 void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override {}
 void recUses(cfgBasicBlock *cBB) override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

/* Coproc 0 type */
class insn_mfc0 : public coprocType0Insn {
 public:
 insn_mfc0(uint32_t inst, uint32_t addr) : coprocType0Insn(inst, addr) {}
 
 uint32_t destRegister() const override {
  return ((inst>>16) & 31);
 }
};
class insn_mtc0 : public coprocType0Insn {
 public:
 insn_mtc0(uint32_t inst, uint32_t addr) : coprocType0Insn(inst, addr) {}
 
 uint32_t destRegister() const override {
  return ~0;
 }
};

/* Coproc 1 type */
class insn_mfc1 : public coprocType1Insn {
protected:
 uint32_t rt;
 public:
 insn_mfc1(uint32_t inst, uint32_t addr) : 
  coprocType1Insn(inst, addr), rt((inst>>16)&31) {}
 
 void recUses(cfgBasicBlock *cBB) override;
 void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
 bool canCompile() const override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
 opPrecType getPrecType() const override {
  return fpspecialprec;
 }
};
class insn_mtc1 : public coprocType1Insn {
protected:
 uint32_t fd, rt;
 public:
 insn_mtc1(uint32_t inst, uint32_t addr) : 
  coprocType1Insn(inst, addr), fd((inst>>11) & 31), rt((inst>>16)&31) {}
 
 void recUses(cfgBasicBlock *cBB) override;
 void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
 bool canCompile() const override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
 opPrecType getPrecType() const override {
  return fpspecialprec;
 }
};


class insn_c : public coprocType1Insn {
protected:
 uint32_t cond, cc;
public:
 insn_c(uint32_t inst, uint32_t addr);
 bool canCompile() const override;
 void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
 void recUses(cfgBasicBlock *cBB) override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class simpleFPType : public coprocType1Insn {
protected:
  enum class fp_insn_type {fadd, fsub, fmul, fdiv};
  fp_insn_type fp_type;
public:
  simpleFPType(uint32_t inst, uint32_t addr, fp_insn_type fp_type) :
    coprocType1Insn(inst, addr), fp_type(fp_type) {}
  bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
  bool canCompile() const override {
    return true;
  }
};

class insn_fadd: public simpleFPType {
 public:
  insn_fadd(uint32_t inst, uint32_t addr) :
    simpleFPType(inst, addr, simpleFPType::fp_insn_type::fadd) {}
};

class insn_fsub: public simpleFPType {
 public:
 insn_fsub(uint32_t inst, uint32_t addr) :
   simpleFPType(inst, addr, simpleFPType::fp_insn_type::fsub) {}
};

class insn_fmul: public simpleFPType {
 public:
 insn_fmul(uint32_t inst, uint32_t addr) :
   simpleFPType(inst, addr, simpleFPType::fp_insn_type::fmul) {}
};

class insn_fdiv: public simpleFPType {
 public:
 insn_fdiv(uint32_t inst, uint32_t addr) :
   simpleFPType(inst, addr, simpleFPType::fp_insn_type::fdiv) {}
};

class insn_fsqrt: public coprocType1Insn {
 public:
 insn_fsqrt(uint32_t inst, uint32_t addr) : coprocType1Insn(inst, addr) {}
 void recUses(cfgBasicBlock *cBB) override;
 bool canCompile() const override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_fmov: public coprocType1Insn {
 public:
 insn_fmov(uint32_t inst, uint32_t addr) : coprocType1Insn(inst, addr) {}
 void recUses(cfgBasicBlock *cBB) override;
 bool canCompile() const override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_truncw: public coprocType1Insn {
 public:
 insn_truncw(uint32_t inst, uint32_t addr) : coprocType1Insn(inst, addr) {}
 void recUses(cfgBasicBlock *cBB) override;
 void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
 bool canCompile() const override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_fmovc: public coprocType1Insn {
protected:
 uint32_t cc, tf;
public:
 insn_fmovc(uint32_t inst, uint32_t addr) : 
  coprocType1Insn(inst, addr), cc((inst >> 18) & 7), tf((inst>>16) & 1) {}
 void recUses(cfgBasicBlock *cBB) override;
 bool canCompile() const override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_fmovz: public coprocType1Insn {
 public:
 insn_fmovz(uint32_t inst, uint32_t addr) : coprocType1Insn(inst, addr) {}
};

class insn_fmovn: public coprocType1Insn {
 public:
 insn_fmovn(uint32_t inst, uint32_t addr) : coprocType1Insn(inst, addr) {}
};

class insn_cvts: public coprocType1Insn {
 public:
 insn_cvts(uint32_t inst, uint32_t addr) : coprocType1Insn(inst, addr) {}
 opPrecType getPrecType() const override {
  return singleprec;
 }
 void recUses(cfgBasicBlock *cBB) override;
 void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
 bool canCompile() const override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

class insn_cvtd: public coprocType1Insn {
public:
 insn_cvtd(uint32_t inst, uint32_t addr) : coprocType1Insn(inst, addr) {}
 opPrecType getPrecType() const override {
  return doubleprec;
 }
 void recUses(cfgBasicBlock *cBB) override;
 void recDefines(cfgBasicBlock *cBB, regionCFG *cfg) override;
 bool canCompile() const override;
 bool generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) override;
};

opPrecType coprocType1xInsn::getPrecType() const {
  switch(mi.c1x.fmt)
    {
    case 0:
      return singleprec;
    case 1:
      return doubleprec;
    default:
      break;
    }
  return unknownprec;
}

bool coprocType1xInsn::canCompile() const {
  return false;
}

void coprocType1xInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->fprDefinitionBlocks[mi.c1x.fd+0].insert(cBB);
  switch(getPrecType())
    {
    case singleprec:
      cBB->updateFPRTouched(mi.c1x.fd, fprUseEnum::singlePrec);
      break;
    case doubleprec:
      cBB->updateFPRTouched(mi.c1x.fd, fprUseEnum::doublePrec);
      break;
    default:
      die();
    }
}

void coprocType1xInsn::recUses(cfgBasicBlock *cBB) {
  cBB->fprRead[mi.c1x.fr]=true;
  cBB->fprRead[mi.c1x.fs]=true;
  cBB->fprRead[mi.c1x.ft]=true;
  switch(getPrecType())
    {
    case singleprec:
      cBB->updateFPRTouched(mi.c1x.fr, fprUseEnum::singlePrec);
      cBB->updateFPRTouched(mi.c1x.fs, fprUseEnum::singlePrec);
      cBB->updateFPRTouched(mi.c1x.ft, fprUseEnum::singlePrec);
      break;
    case doubleprec:
      cBB->updateFPRTouched(mi.c1x.fr, fprUseEnum::doublePrec);
      cBB->updateFPRTouched(mi.c1x.fs, fprUseEnum::doublePrec);
      cBB->updateFPRTouched(mi.c1x.ft, fprUseEnum::doublePrec);
      break;
    default:
      die();
    }
}


std::string Insn::getString() const {
  std::stringstream ss;
  ss << *this;
  return ss.str();
}

static llvm::Value *getConditionCode(regionCFG *cfg, llvmRegTables& regTbl, uint32_t cc) {
  //printf("getcc: condition code id = %u\n", cc);
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  llvm::Value *vOne = llvm::ConstantInt::get(iType32,1);
  llvm::Value *vCC = llvm::ConstantInt::get(iType32,cc);
  llvm::Value *vMask = cfg->myIRBuilder->CreateShl(vOne, vCC);
  llvm::Value *vAnd = cfg->myIRBuilder->CreateAnd(regTbl.fcrTbl[CP1_CR25], vMask);
  llvm::Value *vShr = cfg->myIRBuilder->CreateLShr(vAnd, vCC);
  //std::string getccName = "getcc_" + std::to_string(cfg->getuuid()++);
  return vShr;//cfg->myIRBuilder->CreateAnd(vShr, vOne, getccName);
}

static void setConditionCode(regionCFG *cfg, llvmRegTables& regTbl, 
			     uint32_t cc, llvm::Value *vY) {
  //printf("setcc: condition code id = %u\n", cc);
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  
  llvm::Value *vOne = llvm::ConstantInt::get(iType32,1);
  llvm::Value *vZero = llvm::ConstantInt::get(iType32,0);
  llvm::Value *vCC = llvm::ConstantInt::get(iType32,cc);
  
  vY = cfg->myIRBuilder->CreateSelect(vY, vOne, vZero);

  llvm::Value *m0 = cfg->myIRBuilder->CreateShl(vOne, vCC);
  llvm::Value *m1 = cfg->myIRBuilder->CreateNot(m0);
  llvm::Value *s = cfg->myIRBuilder->CreateSub(vY,vOne);
  llvm::Value *m2 = cfg->myIRBuilder->CreateNot(s);

  llvm::Value *l = cfg->myIRBuilder->CreateAnd(regTbl.fcrTbl[CP1_CR25], m1);
  llvm::Value *r = cfg->myIRBuilder->CreateAnd(m0, m2);

  std::string setccName = "setcc_" + std::to_string(cfg->getuuid()++);
  regTbl.fcrTbl[CP1_CR25] = cfg->myIRBuilder->CreateOr(l, r, setccName);
}

bool Insn::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl){
  die();
  return false;
}

void Insn::updateGPRConstants(std::vector<regState> &gprConstState) {

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

bool Insn::codeGen(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  return generateIR(cBB, nInst, regTbl);
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

void rTypeInsn::updateGPRConstants(std::vector<regState> &gprConstState) {
  if((gprConstState[rs].e == constant) && (gprConstState[rt].e == constant)) {
    gprConstState[rd].e = constant;
    gprConstState[rd].v = doOp(gprConstState[rs].v, gprConstState[rt].v);
  }
  else {
    gprConstState[rd].e = variant;
    gprConstState[rd].v = ~0;
  }
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

coprocType1Insn::coprocType1Insn(uint32_t inst, uint32_t addr) : 
  Insn(inst, addr), fmt((inst >> 21) & 31), ft((inst >> 16) & 31), 
  fs((inst >> 11) & 31), fd((inst >> 6) & 31) {
}

bool coprocType1Insn::canCompile() const {
  return false;
}

void coprocType1Insn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg)  {
  if(fmt == FMT_D) {
    cfg->fprDefinitionBlocks[fd+0].insert(cBB);
    cBB->updateFPRTouched(fd, fprUseEnum::doublePrec);
  }
  else if(fmt == FMT_S) {
    cfg->fprDefinitionBlocks[fd+0].insert(cBB);
    cBB->updateFPRTouched(fd, fprUseEnum::singlePrec);
  }
  else {
    std::cerr << *this << "\n";
    die();
  }
} 

void coprocType1Insn::recUses(cfgBasicBlock *cBB)  {
  cBB->fprRead[fs+0]=true;
  cBB->fprRead[ft+0]=true;
  if(fmt == FMT_D) {
    cBB->updateFPRTouched(fs, fprUseEnum::doublePrec);
    cBB->updateFPRTouched(ft, fprUseEnum::doublePrec);
  }
  else if(fmt == FMT_S) {
    cBB->updateFPRTouched(fs, fprUseEnum::singlePrec);
    cBB->updateFPRTouched(ft, fprUseEnum::singlePrec);
  }
  else {
    std::cerr << *cBB;
    die();
  }

}

bool iBranchTypeInsn::handleBranch(cfgBasicBlock *cBB, Insn* nInst,
				   llvmRegTables& regTbl,
				   llvm::CmpInst::Predicate branchPred,
				   llvm::Value *vRT, llvm::Value *vRS) {
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmp(branchPred, vRS, vRT);

  
  /* generate branch delay slot code */
  nInst->codeGen(cBB, nullptr, regTbl);

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

bool iBranchLikelyTypeInsn::handleBranch(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl,
					 llvm::CmpInst::Predicate branchPred,
					 llvm::Value *vRT, llvm::Value *vRS) {
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmp(branchPred, vRS, vRT);

  
  cfgBasicBlock *patchBB = nullptr;

  for(cfgBasicBlock* b : cBB->succs) {
    if(b->isLikelyPatch) {
      patchBB = b;
      break;
    }
  }
  if(patchBB==nullptr) {
    printf("couldn't find patch basic block for %x\n", cBB->getEntryAddr());
    cBB->print();
    exit(-1);
  }

  llvm::BasicBlock *tBB = patchBB->lBB;
  llvm::BasicBlock *ntBB = cBB->getSuccLLVMBasicBlock(ntAddr);

  ntBB = cfg->generateAbortBasicBlock(ntAddr, regTbl, cBB, ntBB, addr);
  cfg->myIRBuilder->CreateCondBr(vCMP, tBB, ntBB);
    
  cBB->hasTermBranchOrJump = true;

  return false;
}




void iTypeInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  //printf("iTYPE : %s defines %s\n", getAsmString(inst,addr).c_str(), 
  //	 getGPRName(rt, false).c_str());
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
  cBB->gprRead[rs]=true;
  cBB->gprRead[rt]=true;
}
void iTypeStoreInsn::recUses(cfgBasicBlock *cBB) {
  /* stores read both operands */
  cBB->gprRead[rs]=true;
  cBB->gprRead[rt]=true;
}
void iTypeStoreInsn::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  /* don't update anything.. */
}
void insn_jal::recDefines(cfgBasicBlock *cBB, regionCFG *cfg)  {
  cfg->gprDefinitionBlocks[R_ra].insert(cBB);
}

void insn_ldc1::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->fprDefinitionBlocks[ft+0].insert(cBB);
  //cfg->fprDefinitionBlocks[ft+1].insert(cBB);
  cBB->updateFPRTouched(ft, fprUseEnum::doublePrec);
}

void insn_lwc1::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->fprDefinitionBlocks[ft+0].insert(cBB);
  cBB->updateFPRTouched(ft, fprUseEnum::singlePrec);
}

bool insn_lwc1::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  auto Context = cfg->Context;
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context),simm);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA = cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(*Context));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->CreateGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt32PtrTy(*Context));  
  std::string loadName = "lwc1_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vLoad = cfg->myIRBuilder->CreateLoad(vPtr,loadName);
  llvm::Value *vSwap = byteSwap(vLoad);
  llvm::Value *vY = cfg->myIRBuilder->CreateBitCast(vSwap, llvm::Type::getFloatTy(*Context));
  regTbl.setFPR(ft,vY);
  return false;
}

bool insn_ldc1::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  auto Context = cfg->Context;
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context),simm);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA = cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(*Context));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  llvm::Value *vGEP = cfg->myIRBuilder->CreateGEP(vMem, vZEA);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt64PtrTy(*Context));  
  std::string loadName = "lwc1_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vLoad = cfg->myIRBuilder->CreateLoad(vPtr,loadName);
  llvm::Value *vSwap = byteSwap(vLoad);
  llvm::Value *vY = cfg->myIRBuilder->CreateBitCast(vSwap, llvm::Type::getDoubleTy(*Context));
  regTbl.setFPR(ft,vY);
  return false;
}


static const bool enableFPLoads = true;

bool insn_sdc1::canCompile() const {
  return enableFPLoads;
}
void insn_sdc1::recUses(cfgBasicBlock *cBB)  {
  cBB->gprRead[rs]=true;
  cBB->fprRead[ft+0]=true;
  //cBB->fprRead[ft+1]=true;
  cBB->updateFPRTouched(ft, fprUseEnum::doublePrec);
}

bool insn_ldc1::canCompile() const {
  return enableFPLoads;
}

bool insn_swc1::canCompile() const {
  return enableFPLoads;
}

void insn_swc1::recUses(cfgBasicBlock *cBB)  {
  cBB->gprRead[rs]=true;
  cBB->fprRead[ft]=true;
  cBB->updateFPRTouched(ft, fprUseEnum::singlePrec);
}

bool insn_swc1::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  auto Context = cfg->Context;
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context),simm);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vFT = regTbl.getFPR(ft, fprUseEnum::singlePrec);
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA = cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(*Context));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  std::string storeName = "swc1_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vGEP = cfg->myIRBuilder->CreateGEP(vMem, vZEA, storeName);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt32PtrTy(*Context));  
  llvm::Value *vCast = cfg->myIRBuilder->CreateBitCast(vFT, llvm::Type::getInt32Ty(*Context));
  llvm::Value *vSwap = byteSwap(vCast);
  cfg->myIRBuilder->CreateStore(vSwap, vPtr);
  return false;
}


bool insn_sdc1::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  auto Context = cfg->Context;
  llvm::Value *vIMM = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context),simm);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vFT = regTbl.getFPR(ft,fprUseEnum::doublePrec);
  llvm::Value *vEA = cfg->myIRBuilder->CreateAdd(vRS, vIMM);
  llvm::Value *vZEA = cfg->myIRBuilder->CreateZExt(vEA, llvm::Type::getInt64Ty(*Context));
  llvm::Value *vMem = cfg->blockArgMap["mem"];
  std::string storeName = "sdc1_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  llvm::Value *vGEP = cfg->myIRBuilder->CreateGEP(vMem, vZEA, storeName);
  llvm::Value *vPtr = cfg->myIRBuilder->CreateBitCast(vGEP, llvm::Type::getInt64PtrTy(*Context));  
  llvm::Value *vCast = cfg->myIRBuilder->CreateBitCast(vFT, llvm::Type::getInt64Ty(*Context));
  llvm::Value *vSwap = byteSwap(vCast);
  cfg->myIRBuilder->CreateStore(vSwap, vPtr);
  return false;
}

bool insn_lwc1::canCompile() const {
  return enableFPLoads;
}

void insn_mul::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[rd].insert(cBB);
}
void insn_mul::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[rs] = true;
  cBB->gprRead[rt] = true;
}
bool insn_mul::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl)
{
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType64 = llvm::Type::getInt64Ty(cxt);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vRT = regTbl.gprTbl[rt];
  llvm::Value *vZRS = cfg->myIRBuilder->CreateSExt(vRS, iType64);
  llvm::Value *vZRT = cfg->myIRBuilder->CreateSExt(vRT, iType64);
  llvm::Value *vMul = cfg->myIRBuilder->CreateMul(vZRS,vZRT);
  llvm::Value *vLo = cfg->myIRBuilder->CreateTrunc(vMul, iType32);
  regTbl.gprTbl[rd] = vLo;
  return false;
}

void insn_mul::updateGPRConstants(std::vector<regState> &gprConstState) {
  if((gprConstState[rs].e == constant) && (gprConstState[rt].e == constant)) {
    gprConstState[rd].e = constant;
    gprConstState[rd].v = (gprConstState[rs].v *  gprConstState[rt].v);
  }
  else {
    gprConstState[rd].e = variant;
    gprConstState[rd].v = ~0;
  }
}

void insn_clz::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[rd].insert(cBB);
}
void insn_clz::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[rs] = true;
}

bool insn_clz::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Value *vTrue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(cxt),1);
  std::vector<llvm::Type*> typeVec;
  llvm::Value *vRS = regTbl.gprTbl[rs];
  typeVec.push_back(vRS->getType());
  llvm::ArrayRef<llvm::Type*> typeArrRef(typeVec);
  auto vclzIntr = llvm::Intrinsic::getDeclaration(cfg->myModule, llvm::Intrinsic::ctlz, typeArrRef);
  std::vector<lv_t*> args;
  args.push_back(vRS);
  args.push_back(vTrue);
  llvm::ArrayRef<lv_t*> argRef(args);
  regTbl.gprTbl[rd] = cfg->myIRBuilder->CreateCall(vclzIntr, argRef);
  return false;
}



void insn_ext::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[rt].insert(cBB);
}

void insn_ins::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[rt].insert(cBB);
}

void insn_seb::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[rd].insert(cBB);
}


bool insn_ext::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  uint32_t pos = (inst >> 6) & 31;
  uint32_t size = ((inst >> 11) & 31) + 1;
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt); 
  llvm::Value *vSize = llvm::ConstantInt::get(iType32,size);
  llvm::Value *vPos = llvm::ConstantInt::get(iType32,pos);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vOne = llvm::ConstantInt::get(iType32,1);
  llvm::Value *vShftL = cfg->myIRBuilder->CreateShl(vOne, vSize);
  llvm::Value *vShftR = cfg->myIRBuilder->CreateLShr(vRS, vPos);
  llvm::Value *vShftLM = cfg->myIRBuilder->CreateSub(vShftL, vOne);
  llvm::Value *vAnd = cfg->myIRBuilder->CreateAnd(vShftR,vShftLM);
  regTbl.gprTbl[rt] = vAnd;
  return false;
}


void insn_seh::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->gprDefinitionBlocks[rd].insert(cBB);
}


bool insn_seb::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Value *vRT = regTbl.gprTbl[rt];
  llvm::Value *vTrunc = cfg->myIRBuilder->CreateTrunc(vRT, llvm::Type::getInt8Ty(cxt));
  regTbl.gprTbl[rd] =  cfg->myIRBuilder->CreateSExt(vTrunc, llvm::Type::getInt32Ty(cxt));
  return false;
}


void insn_ext::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[rs]=true;
}


void insn_seb::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[rt]=true;
}

void insn_seh::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[rt]=true;
}

bool insn_seh::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Value *vRT = regTbl.gprTbl[rt];
  llvm::Value *vTrunc = cfg->myIRBuilder->CreateTrunc(vRT, llvm::Type::getInt16Ty(cxt));
  regTbl.gprTbl[rd] = cfg->myIRBuilder->CreateSExt(vTrunc, llvm::Type::getInt32Ty(cxt));
  return false;
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



uint32_t simpleRType::doOp(uint32_t a, uint32_t b) {
  switch(r_type)
    {
    case rtype::addu:
      return a+b;
    case rtype::subu:
      return a-b;
    case rtype::or_:
      return a|b;
    case rtype::and_:
      return a&b;
    case rtype::nor_:
      return ~(a|b);
    case rtype::xor_:
      return a^b;
    case rtype::sll:
      return a << ((inst>>6)&31);
    case rtype::srl:
      return a >> ((inst>>6)&31);
    case rtype::srlv:
      return a>>b;
    case rtype::sllv:
      return a<<b;
    case rtype::sra:
      return static_cast<int32_t>(a) << ((inst >> 6) & 31);
    case rtype::srav:
      return static_cast<int32_t>(a) << b;
    default:
      assert(false);
    }
  return 0;
}

bool insn_movci::generateIR(cfgBasicBlock *cBB, Insn* nInst,  llvmRegTables& regTbl) {
  uint32_t cc = (inst >> 18) & 7, tf = (inst>>16) & 1;
  llvm::Value *vZ = llvm::ConstantInt::get(cfg->type_int32,0);
  llvm::Value *vCode = getConditionCode(cfg,regTbl, cc);
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmpEQ(vCode, vZ);
  llvm::Value *vRD = regTbl.gprTbl[rd];
  llvm::Value *vRS = regTbl.gprTbl[rs];
  if(tf==0)
    vRD = cfg->myIRBuilder->CreateSelect(vCMP, vRS, vRD);
  else
    vRD = cfg->myIRBuilder->CreateSelect(vCMP, vRD, vRS);
  regTbl.gprTbl[rd] = vRD; 
  return false;
}

void insn_movci::recUses(cfgBasicBlock *cBB) {
  cBB->fcrRead[CP1_CR25]=true;
  cBB->gprRead[rd]=true;
  cBB->gprRead[rs]=true;

}

void insn_movci::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  if(rd != 0)
    cfg->gprDefinitionBlocks[rd].insert(cBB);
}


bool simpleIType::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
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

bool simpleRType::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
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

void insn_sll::updateGPRConstants(std::vector<regState> &gprConstState) {
  if(gprConstState[rt].e == constant) {
    gprConstState[rd].e = constant;
    gprConstState[rd].v = doOp(gprConstState[rt].v, ~0);
  }
  else {
    gprConstState[rd].e = variant;
    gprConstState[rd].v = ~0;
  }
}
  
  
void insn_srl::updateGPRConstants(std::vector<regState> &gprConstState) {
  if(gprConstState[rt].e == constant) {
    gprConstState[rd].e = constant;
    gprConstState[rd].v = doOp(gprConstState[rt].v, ~0);
  }
  else {
    gprConstState[rd].e = variant;
    gprConstState[rd].v = ~0;
  }
}  

void insn_sra::updateGPRConstants(std::vector<regState> &gprConstState) {
  if(gprConstState[rt].e == constant) {
    gprConstState[rd].e = constant;
    gprConstState[rd].v = doOp(gprConstState[rt].v, ~0);
  }
  else {
    gprConstState[rd].e = variant;
    gprConstState[rd].v = ~0;
  }
}


  
void insn_addiu::updateGPRConstants(std::vector<regState> &gprConstState) {
  if(gprConstState[rs].e == constant) {
    gprConstState[rt].e = constant;
    gprConstState[rt].v = gprConstState[rs].v + (uimm<<16);
  }
  else {
    gprConstState[rt].e = variant;
    gprConstState[rt].v = ~0;
  }
}

bool insn_sb::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
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

bool insn_sh::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
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



bool insn_sw::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
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


bool insn_lbu::generateIR(cfgBasicBlock *cBB, Insn *nInst,llvmRegTables& regTbl) {
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

bool insn_lb::generateIR(cfgBasicBlock *cBB, Insn *nInst,llvmRegTables& regTbl) {
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

bool insn_lh::generateIR(cfgBasicBlock *cBB, Insn *nInst,llvmRegTables& regTbl) {
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

bool insn_lhu::generateIR(cfgBasicBlock *cBB, Insn *nInst,llvmRegTables& regTbl) {
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

bool insn_lw::generateIR(cfgBasicBlock *cBB, Insn *nInst,llvmRegTables& regTbl) {
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

void insn_lui::updateGPRConstants(std::vector<regState> &gprConstState) {
  gprConstState[rt].e = constant;
  gprConstState[rt].v = uimm<<16;
}

void insn_andi::updateGPRConstants(std::vector<regState> &gprConstState) {
    if(gprConstState[rs].e == constant) {
      gprConstState[rt].e = constant;
      gprConstState[rt].v = gprConstState[rs].v + (uimm<<16);
    }
    else {
      gprConstState[rt].e = variant;
      gprConstState[rt].v = ~0;
    }
}

void insn_xori::updateGPRConstants(std::vector<regState> &gprConstState) {
  if(gprConstState[rs].e == constant) {
    gprConstState[rt].e = constant;
    gprConstState[rt].v = gprConstState[rs].v ^ (uimm<<16);
  }
  else {
    gprConstState[rt].e = variant;
    gprConstState[rt].v = ~0;
  }
}

void insn_ori::updateGPRConstants(std::vector<regState> &gprConstState) {
  if(gprConstState[rs].e == constant) {
    gprConstState[rt].e = constant;
    gprConstState[rt].v = gprConstState[rs].v | (uimm<<16);
  }
  else {
    gprConstState[rt].e = variant;
    gprConstState[rt].v = ~0;
  }
}

bool insn_slti::generateIR(cfgBasicBlock *cBB, Insn *nInst,  llvmRegTables& regTbl) {
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

bool insn_sltiu::generateIR(cfgBasicBlock *cBB, Insn *nInst,  llvmRegTables& regTbl) {
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

bool insn_bltu::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Type *iType32 = llvm::Type::getInt32Ty(cxt);
  llvm::Value *vRT = llvm::ConstantInt::get(iType32,0);
  llvm::Value *vRS = regTbl.gprTbl[rs];
  return handleBranch(cBB, nInst, regTbl,llvm::CmpInst::ICMP_SLT, vRT, vRS);
}

bool insn_bne::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
  llvm::Value *vRT = regTbl.gprTbl[rt];
  llvm::Value *vRS = regTbl.gprTbl[rs];
  return handleBranch(cBB,nInst,regTbl,llvm::CmpInst::ICMP_NE,vRT,vRS);
}


bool insn_beq::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
  llvm::Value *vRT = regTbl.gprTbl[rt];
  llvm::Value *vRS = regTbl.gprTbl[rs];
  return handleBranch(cBB, nInst, regTbl,llvm::CmpInst::ICMP_EQ, vRT, vRS);
}


bool insn_j::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
  llvm::BasicBlock *tBB = cBB->getSuccLLVMBasicBlock(jaddr);

  nInst->codeGen(cBB, nullptr, regTbl);
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

bool insn_jal::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {  
  llvm::LLVMContext &cxt = *(cfg->Context);
  regTbl.gprTbl[31] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cxt),(addr+8));
  nInst->codeGen(cBB, nullptr, regTbl);
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


bool insn_jr::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
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
  nInst->codeGen(cBB, nullptr, regTbl);

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

bool insn_jalr::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
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
  
  nInst->codeGen(cBB, nullptr, regTbl);

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


bool insn_movn::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Value *vRT = regTbl.gprTbl[rt];
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vRD = regTbl.gprTbl[rd];

  llvm::Value *vZ = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cxt),0);
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmpNE(vRT, vZ);
  regTbl.gprTbl[rd] = cfg->myIRBuilder->CreateSelect(vCMP,vRS,vRD); 

  return false;
}

bool insn_movz::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
  llvm::LLVMContext &cxt = *(cfg->Context);
  llvm::Value *vRT = regTbl.gprTbl[rt];
  llvm::Value *vRS = regTbl.gprTbl[rs];
  llvm::Value *vRD = regTbl.gprTbl[rd];

  llvm::Value *vZ = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cxt),0);
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmpEQ(vRT, vZ);
  regTbl.gprTbl[rd] = cfg->myIRBuilder->CreateSelect(vCMP,vRS,vRD); 

  return false;
}

bool insn_teq::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
  return false;
}

bool insn_tge::generateIR(cfgBasicBlock *cBB, Insn *nInst, llvmRegTables& regTbl) {
  return false;
}

opPrecType coprocType1Insn::getPrecType() const {
  switch(fmt) {
  case FMT_S:
    return singleprec;
    break;
  case FMT_D:
    return doubleprec;
    break;
  default: 
    {
      printf("has unk prec -> %s\n", getAsmString(inst, addr).c_str());
      return unknownprec;
      break;
    }
  }
}

bool insn_c::canCompile() const {
  return true;
}
bool insn_fmov::canCompile() const {
  return true;
}
bool insn_fmovc::canCompile() const {
  return true;
}
bool insn_fsqrt::canCompile() const {
  return true;
}
bool insn_mtc1::canCompile() const {
  return true;
}

bool insn_cvts::canCompile() const {
  return true;
}
bool insn_cvtd::canCompile() const {
  return true;
}
bool insn_truncw::canCompile() const {
  return true;
}


insn_c::insn_c(uint32_t inst, uint32_t addr) : 
  coprocType1Insn(inst, addr), cond(inst & 15), cc((inst>>8)&7) {
}


void insn_c::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->fcrDefinitionBlocks[CP1_CR25].insert(cBB);
}

void insn_c::recUses(cfgBasicBlock *cBB) {
  coprocType1Insn::recUses(cBB);
  cBB->fcrRead[CP1_CR25]=true;
}

bool insn_c::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::Value *vFS = regTbl.getFPR(fs, TC);
  llvm::Value *vFT = regTbl.getFPR(ft, TC);
  llvm::Value *vY = nullptr;
  switch(cond)
    {
    case COND_EQ:
      vY = cfg->myIRBuilder->CreateFCmpOEQ(vFS,vFT);
      break;
    case COND_LT:
      vY = cfg->myIRBuilder->CreateFCmpOLT(vFS,vFT);
      break;
    case COND_LE:
      vY = cfg->myIRBuilder->CreateFCmpOLE(vFS,vFT);
      break;
    case COND_ULE:      
      vY = cfg->myIRBuilder->CreateFCmpULE(vFS,vFT);
      break;
    default:
      printf("unimplemented %s = %s\n", __func__, getCondName(cond).c_str());
      exit(-1);
      break;
    }
  dbt_assert(vY);
  setConditionCode(cfg,regTbl, cc, vY);

  return false;
}

bool simpleFPType::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::Value *vY = nullptr;
  switch(fp_type)
    {
    case fp_insn_type::fadd:
      vY = cfg->myIRBuilder->CreateFAdd(regTbl.getFPR(fs,TC), regTbl.getFPR(ft,TC));
      break;
    case fp_insn_type::fsub:
      vY = cfg->myIRBuilder->CreateFSub(regTbl.getFPR(fs,TC), regTbl.getFPR(ft,TC));
      break;
    case fp_insn_type::fdiv:
      vY = cfg->myIRBuilder->CreateFDiv(regTbl.getFPR(fs,TC), regTbl.getFPR(ft,TC));
      break;
    case fp_insn_type::fmul:
      vY = cfg->myIRBuilder->CreateFMul(regTbl.getFPR(fs,TC), regTbl.getFPR(ft,TC));
      break;
    default:
      assert(false);
    }
  regTbl.setFPR(fd, vY);
  return false;
}

void insn_fmov::recUses(cfgBasicBlock *cBB) {
  if(fmt == FMT_D) {
    cBB->fprRead[fs+0]=true; //cBB->fprRead[fs+1]=true;
    cBB->updateFPRTouched(fs, fprUseEnum::doublePrec);
  }
  else if(fmt == FMT_S) {
    cBB->fprRead[fs+0]=true;
    cBB->updateFPRTouched(fs, fprUseEnum::singlePrec);
  }
  else {
    die();
  }
}


bool insn_fmov::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  regTbl.setFPR(fd, regTbl.getFPR(fs, TC));
  return false;
}

void insn_mfc1::recUses(cfgBasicBlock *cBB) {
  cBB->fprRead[fs+0]=true;
  cBB->updateFPRTouched(fs, fprUseEnum::singlePrec);
}

void insn_mfc1::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  if(rt != 0)
    cfg->gprDefinitionBlocks[rt].insert(cBB);
}

bool insn_mfc1::canCompile() const {
  return true;
}

bool insn_mfc1::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::Type *iType32 = llvm::Type::getInt32Ty(*cfg->Context);
  llvm::Value *vFD = regTbl.getFPR(fs, fprUseEnum::singlePrec);
  regTbl.gprTbl[rt] = cfg->myIRBuilder->CreateBitCast(vFD,iType32);
  return false;
}

void insn_mtc1::recUses(cfgBasicBlock *cBB) {
  cBB->gprRead[rt]=true;
  cBB->updateFPRTouched(fd, fprUseEnum::singlePrec);
}

void insn_mtc1::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->fprDefinitionBlocks[fd].insert(cBB);
}


bool insn_mtc1::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::Type *fType = llvm::Type::getFloatTy(*cfg->Context);
  llvm::Value *vRT = regTbl.gprTbl[rt];
  llvm::Value *Y = nullptr;
  if(rt==0) {
    Y = llvm::ConstantFP::get(fType,0.0);
  }
  else {
    Y = cfg->myIRBuilder->CreateBitCast(vRT,fType);
  }
  Y->setName(("mtc1_" + toStringHex(addr)));
  regTbl.setFPR(fd,Y);
  return false;
}



void insn_cvts::recUses(cfgBasicBlock *cBB) {
  if(fmt == FMT_D) {
    cBB->fprRead[fs+0]=true; //cBB->fprRead[fs+1]=true;
    cBB->updateFPRTouched(fs, fprUseEnum::doublePrec);
  }
  else if(fmt == FMT_W) {
    cBB->fprRead[fs+0]=true;
    cBB->updateFPRTouched(fs, fprUseEnum::singlePrec);
  }
  else {
    die();
  }
}


void insn_cvts::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->fprDefinitionBlocks[fd+0].insert(cBB);
  cBB->updateFPRTouched(fd, fprUseEnum::singlePrec);
}


bool insn_cvts::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::Value *vFS = nullptr, *vFD = nullptr;
  std::string  n = "cvts_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);
  if(fmt == FMT_D) {
    vFS = regTbl.getFPR(fs, fprUseEnum::doublePrec);
    vFD = cfg->myIRBuilder->CreateFPCast(vFS,cfg->type_float, n);
  }
  else if(fmt == FMT_W) {
    vFS = regTbl.getFPR(fs, fprUseEnum::singlePrec);
    vFS = cfg->myIRBuilder->CreateBitCast(vFS,cfg->type_int32);
    vFD = cfg->myIRBuilder->CreateSIToFP(vFS,cfg->type_float,n);
  }
  else {
    die();
  }

  regTbl.setFPR(fd,vFD);
  return false;
}

void insn_cvtd::recUses(cfgBasicBlock *cBB) {
  if(fmt == FMT_S) {
    cBB->fprRead[fs+0]=true; 
    cBB->updateFPRTouched(fs, fprUseEnum::singlePrec);
  }
  else if(fmt == FMT_W) {
    cBB->fprRead[fs+0]=true;
    cBB->updateFPRTouched(fs, fprUseEnum::singlePrec);
  }
  else {
    die();
  }
}


void insn_cvtd::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->fprDefinitionBlocks[fd+0].insert(cBB);
  cBB->updateFPRTouched(fd, fprUseEnum::doublePrec);
}


bool insn_cvtd::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::Value *vFS = regTbl.getFPR(fs, fprUseEnum::singlePrec), *vFD = nullptr;
  std::string n = "cvtd_" + std::to_string(cfg->getuuid()++) + "_" + toStringHex(addr);

  
  switch(fmt) 
    {
    case FMT_W: {
      vFS = cfg->myIRBuilder->CreateBitCast(vFS,cfg->type_int32);
#if 0
      std::vector<llvm::Value*> argVector;
      argVector.push_back(vFS);
      llvm::ArrayRef<llvm::Value*> cArgs(argVector);
      cfg->myIRBuilder->CreateCall(cfg->builtinFuncts["print_int32"],cArgs);
#endif
      vFD = cfg->myIRBuilder->CreateSIToFP(vFS,cfg->type_double,n);
      break;
    }
    case FMT_S: {
#if 0
      std::vector<llvm::Value*> argVector;
      argVector.push_back(vFS);
      llvm::ArrayRef<llvm::Value*> cArgs(argVector);
      cfg->myIRBuilder->CreateCall(cfg->builtinFuncts["print_float"],cArgs);
#endif
      vFD = cfg->myIRBuilder->CreateFPCast(vFS,cfg->type_double,n);
      break;
    }
    default:
      die();
    }
  
#if 0
  std::vector<llvm::Value*> argVector;
  argVector.push_back(vFD);
  llvm::ArrayRef<llvm::Value*> cArgs(argVector);
  cfg->myIRBuilder->CreateCall(cfg->builtinFuncts["print_double"],cArgs);
#endif
  
  regTbl.setFPR(fd,vFD);
  return false;
}


void insn_fsqrt::recUses(cfgBasicBlock *cBB) {
  if(fmt == FMT_D) {
    cBB->fprRead[fs+0]=true; //cBB->fprRead[fs+1]=true;
    cBB->updateFPRTouched(fs, fprUseEnum::doublePrec);
  }
  else if(fmt == FMT_S) {
    cBB->fprRead[fs+0]=true;
    cBB->updateFPRTouched(fs, fprUseEnum::singlePrec);
  }
  else {
    die();
  }
}



bool insn_fsqrt::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::Value *vT = regTbl.getFPR(fs, TC);
  std::vector<llvm::Type*> typeVec;
  typeVec.push_back(vT->getType());
  llvm::ArrayRef<llvm::Type*> typeArrRef(typeVec);

  auto vSqrt = llvm::Intrinsic::getDeclaration(cfg->myModule,
					       llvm::Intrinsic::sqrt, 
					       typeArrRef);
  vT  = cfg->myIRBuilder->CreateCall(vSqrt, regTbl.getFPR(fs, TC));
  regTbl.setFPR(fd, vT);
  return false;
}

void insn_fmovc::recUses(cfgBasicBlock *cBB) {
  cBB->fcrRead[CP1_CR25]=true;
  if(fmt == FMT_D) {
    cBB->fprRead[fs+0]=true; //cBB->fprRead[fs+1]=true;
    cBB->fprRead[fd+0]=true; //cBB->fprRead[fd+1]=true;
    cBB->updateFPRTouched(fs, fprUseEnum::doublePrec);
    cBB->updateFPRTouched(fd, fprUseEnum::doublePrec);
  }
  else if(fmt == FMT_S) {
    cBB->fprRead[fs+0]=true;
    cBB->fprRead[fd+0]=true;
    cBB->updateFPRTouched(fs, fprUseEnum::singlePrec);
    cBB->updateFPRTouched(fd, fprUseEnum::singlePrec);
  }
  else {
    die();
  }
}


bool insn_fmovc::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::Type *iType32 = llvm::Type::getInt32Ty(*cfg->Context);
  llvm::Value *vOne = llvm::ConstantInt::get(iType32,1);

  llvm::Value *vCode = getConditionCode(cfg,regTbl, cc);
  
  llvm::Value *vCMP = cfg->myIRBuilder->CreateICmpEQ(vCode, vOne);
  llvm::Value *vFD = regTbl.getFPR(fd, TC);
  llvm::Value *vFS = regTbl.getFPR(fs, TC);
  
  if(tf==0)
    regTbl.setFPR(fd, cfg->myIRBuilder->CreateSelect(vCMP, vFD, vFS));
  else
    regTbl.setFPR(fd, cfg->myIRBuilder->CreateSelect(vCMP, vFS, vFD));
    
  
  return false;
}

void insn_truncw::recUses(cfgBasicBlock *cBB) {
  if(fmt == FMT_D) {
    cBB->fprRead[fs+0]=true; //cBB->fprRead[fs+1]=true;
    cBB->updateFPRTouched(fs, fprUseEnum::doublePrec);
  }
  else if(fmt == FMT_S) {
    cBB->fprRead[fs+0]=true;
    cBB->updateFPRTouched(fs, fprUseEnum::singlePrec);
  }
  else {
    std::cout << "fmt = " << fmt << "\n";
    die();
  }
}

void insn_truncw::recDefines(cfgBasicBlock *cBB, regionCFG *cfg) {
  cfg->fprDefinitionBlocks[fd+0].insert(cBB);
  cBB->updateFPRTouched(fd, fprUseEnum::singlePrec);
} 


bool insn_truncw::generateIR(cfgBasicBlock *cBB, Insn* nInst, llvmRegTables& regTbl) {
  llvm::Value *vFS = regTbl.getFPR(fs, TC);
  llvm::Value *vFD = cfg->myIRBuilder->CreateFPToSI(vFS,cfg->type_int32);
  vFD = cfg->myIRBuilder->CreateBitCast(vFD, cfg->type_float);
  regTbl.setFPR(fd, vFD);
  return false;
}
