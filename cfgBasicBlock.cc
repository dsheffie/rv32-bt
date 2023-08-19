#include <ostream>
#include <fstream>

#include "regionCFG.hh"
#include "helper.hh"
#include "globals.hh"

cfgBasicBlock::~cfgBasicBlock() {
  for(size_t i = 0; i < insns.size(); i++) 
    delete insns[i];
  for(size_t i = 0; i < phiNodes.size(); i++) 
    delete phiNodes[i];
}

void cfgBasicBlock::addWithInCFGEdges(regionCFG *cfg) {
  /* if this block has a branch, search if successor
   * is in CFG region */
  iBranchTypeInsn *iBranch = nullptr;
  insn_jr *jr = nullptr;
  insn_jalr *jalr = nullptr;
  insn_j *j = nullptr;
  insn_jal *jal = nullptr;

  std::map<uint32_t,cfgBasicBlock*>::iterator mit0,mit1;
  uint32_t tAddr=~0,ntAddr=~0;

  /* this is crappy code */
  for(ssize_t i = insns.size()-1; i >= 0; i--) {
    iBranch = dynamic_cast<iBranchTypeInsn*>(insns[i]);
    if(iBranch)
      break;
    jr = dynamic_cast<insn_jr*>(insns[i]);
    if(jr)
      break;
    jal = dynamic_cast<insn_jal*>(insns[i]);
    if(jal)
      break;
    jalr = dynamic_cast<insn_jalr*>(insns[i]);
    if(jalr)
      break;
    j = dynamic_cast<insn_j*>(insns[i]);
    if(j)
      break;
	
    //jJump = dynamic_cast<jTypeInsn*>(insns[i]);
    //if(jJump)
    //break;
    //jrInsn = dynamic_cast<rTypeJumpRegInsn*>(insns[i]);
    //if(jrInsn) break;
  }
  
#define PRINT_ADDED_EDGE(TGT) {\
    std::cerr << __PRETTY_FUNCTION__ << "@" << __LINE__ << " edge : " \
	      << std::hex << getEntryAddr() << " -> " << TGT->getEntryAddr() << std::dec << "\n"; \
  }
  
  if(iBranch) {
    tAddr = iBranch->getTakenAddr();
    ntAddr = iBranch->getNotTakenAddr();
    mit0 = cfg->cfgBlockMap.find(tAddr);
    mit1 = cfg->cfgBlockMap.find(ntAddr);
    if(mit0 != cfg->cfgBlockMap.end()) {
      PRINT_ADDED_EDGE(mit0->second);
      assert(mit0->second->getEntryAddr() == tAddr);
      addSuccessor(mit0->second); 
    }
    if(mit1 != cfg->cfgBlockMap.end()) {
      PRINT_ADDED_EDGE(mit1->second);
      assert(mit1->second->getEntryAddr() == ntAddr);
      addSuccessor(mit1->second); 
    }
  }
  
  //else if(j!=nullptr or jal != nullptr) {
  //tAddr = j ? j->getJumpAddr() : jal->getJumpAddr();
  //mit0 = cfg->cfgBlockMap.find(tAddr);
  //if(mit0 != cfg->cfgBlockMap.end()) {
  //PRINT_ADDED_EDGE(mit0->second);
  //addSuccessor(mit0->second);
  // }
  //}
  
  /* end-of-block with no branch or jump */
  // else {
  //   tAddr = getExitAddr()+4;
  //   mit0 = cfg->cfgBlockMap.find(tAddr);
  //   if(mit0 != cfg->cfgBlockMap.end()) {
  //     PRINT_ADDED_EDGE(mit0->second);
  //     addSuccessor(mit0->second);
  //   }
  // }

}

bool cfgBasicBlock::checkIfPlausableSuccessors() {
  /* find terminating instruction */
  iBranchTypeInsn *iBranch = nullptr;
  for(size_t i = 0,n=insns.size(); i < n; i++) {
    iBranch = dynamic_cast<iBranchTypeInsn*>(insns[i]);
    if(iBranch) 
      break;
  }
  if(!iBranch)
    return true;

  bool impossible = false;


  for(auto cbb : succs) {
    if(cbb->insns.empty())
      continue;
    Insn *ins = cbb->insns[0];
    uint32_t eAddr = ins->getAddr();
    if(iBranch->getTakenAddr() == eAddr || iBranch->getNotTakenAddr() == eAddr)
      continue;
    impossible |= true;
  }
  
  return not(impossible);
}

void cfgBasicBlock::addSuccessor(cfgBasicBlock *s) {
  succs.insert(s);
  s->preds.insert(this);
}

void cfgBasicBlock::delSuccessor(cfgBasicBlock *s) {
  std::set<cfgBasicBlock*>::iterator sit = succs.find(s);
  succs.erase(sit);
  sit = s->preds.find(this);
  s->preds.erase(sit);
}

void cfgBasicBlock::addPhiNode(gprPhiNode *phi) {
  uint32_t r = phi->destRegister();
  if(gprPhis[r])
    delete phi;
  else {
    phiNodes.push_back(phi);
    gprPhis[r] = phi;
  }
}

void cfgBasicBlock::addPhiNode(fprPhiNode *phi) {
  uint32_t r = phi->destRegister();
  if(fprPhis[r])
    delete phi;
  else {
    phiNodes.push_back(phi);
    fprPhis[r] = phi;
  }
}
void cfgBasicBlock::addPhiNode(fcrPhiNode *phi) {
  uint32_t r = phi->destRegister();
  if(fcrPhis[r])
    delete phi;
  else {
    phiNodes.push_back(phi);
    fcrPhis[r] = phi;
  }
}
void cfgBasicBlock::addPhiNode(icntPhiNode *phi) {
  if(icntPhis[0])
    delete phi;
  else {
    phiNodes.push_back(phi);
    icntPhis[0] = phi;
  }
}


bool cfgBasicBlock::has_jr_jalr() {
  for(size_t i = 0; i < insns.size(); i++) {
    Insn *ins = insns[i];
    if(dynamic_cast<insn_jr*>(ins))
      return true;
    if(dynamic_cast<insn_jalr*>(ins))
      return true;
  }
  return false;
}


bool cfgBasicBlock::hasFloatingPoint(uint32_t *typeCnts) const {
  bool hasFP = false;
  for(const Insn* I : insns) {
    opPrecType oType = I->getPrecType();
    hasFP |= (oType==singleprec);
    hasFP |= (oType==doubleprec);
    typeCnts[oType]++;
  }
  return hasFP;
}

bool cfgBasicBlock::canCompile() const {
  for(size_t i = 0, len = insns.size(); i < len; i++) {
    if(not(insns[i]->canCompile())) {
      return false;
    }
  }
  return true;
}

uint32_t cfgBasicBlock::getExitAddr() const {
  return insns.empty() ? ~(0U) : insns.at(insns.size()-1)->getAddr();
}

std::string cfgBasicBlock::getName() const {
  return insns.empty() ? std::string("ENTRY") : toStringHex(insns.at(0)->getAddr());
}

std::ostream &operator<<(std::ostream &out, const cfgBasicBlock &bb) {
  out << "Successors of block: ";
  for(auto cBB : bb.succs) {
    if(!cBB->insns.empty()) {
      stream_hex(out, cBB->insns[0]->getAddr());
    }
    else
      out << "ENTRY ";
  }
  out << std::endl;
  out << "Predecessors of block: ";
  for(auto cBB : bb.preds) {
    if(!cBB->insns.empty()) {
      stream_hex(out,cBB->insns[0]->getAddr()); 
    }
    else
      out << "ENTRY ";
  }
  out << std::endl;
  for(auto ins : bb.insns) {
    out << *ins;
  }
  return out;
}


void cfgBasicBlock::print() {
  std::cerr << *this << std::endl;
}






cfgBasicBlock* cfgBasicBlock::splitBB(uint32_t splitpc) {
  ssize_t offs = -1;
#if 0
  std::cout << "old:\n";
  for(size_t i = 0, n = rawInsns.size(); i < n; i++) {
    std::cout << std::hex << rawInsns.at(i).second << std::dec << " : ";
    disassemble(std::cout, rawInsns.at(i).first, rawInsns.at(i).second);
    std::cout << "\n";
  }
#endif
  
  cfgBasicBlock *sbb = new cfgBasicBlock(bb);
  sbb->rawInsns.clear();
  sbb->hasTermBranchOrJump = hasTermBranchOrJump;

  for(size_t i = 0, n = rawInsns.size(); i < n; i++) {
    if(rawInsns.at(i).second == splitpc) {
      offs = i;
      break;
    }
  }
  //std::cout << "offset @ " << offs << "\n";
  assert(offs != -1);
  
  for(size_t i = offs, n = rawInsns.size(); i < n; i++) {
    sbb->rawInsns.push_back(rawInsns.at(i));
  }
  rawInsns.erase(rawInsns.begin()+offs,rawInsns.end());
  
#if 0
  std::cout << "split at pc " << std::hex << splitpc << std::dec << "\n";
  std::cout << "split 0:\n";
  for(size_t i = 0, n = rawInsns.size(); i < n; i++) {
    std::cout << std::hex << rawInsns.at(i).second << std::dec << " : ";
    disassemble(std::cout, rawInsns.at(i).first, rawInsns.at(i).second);
    std::cout << "\n";
  }
  
  std::cout << "split 1:\n";
  for(size_t i = 0, n = sbb->rawInsns.size(); i < n; i++) {
    std::cout << std::hex << sbb->rawInsns.at(i).second << std::dec << " : ";
    disassemble(std::cout, sbb->rawInsns.at(i).first, sbb->rawInsns.at(i).second);
    std::cout << "\n";
  }
#endif
  
  /* attribute all successors to sbb */
  for(cfgBasicBlock *nbb : succs) {
    //std::cout << "succ @ " << std::hex <<  nbb->getEntryAddr() << std::dec << "\n";
    auto it = nbb->preds.find(this);
    assert(it != nbb->preds.end());
    nbb->preds.erase(it);
    sbb->addSuccessor(nbb);
  }
  succs.clear();
#if 0
  std::cout << std::hex
	    << getEntryAddr()
	    << " has a succ @ "
	    <<  sbb->getEntryAddr()
	    << std::dec
	    << "\n";
#endif
  addSuccessor(sbb);

  assert(succs.size() == 1);
#if 0
  for(cfgBasicBlock *nbb : succs) {
    std::cout << std::hex << getEntryAddr()
	      << " has a succ @ "  <<  nbb->getEntryAddr() << std::dec << "\n";
  }
#endif
  hasTermBranchOrJump = false;
  
  return sbb;
}

cfgBasicBlock::cfgBasicBlock(basicBlock *bb) :
  bb(bb),
  hasTermBranchOrJump(false),
  lBB(nullptr),
  idombb(nullptr) {
  
  fprTouched.resize(32, fprUseEnum::unused);
  gprPhis.fill(nullptr);
  fprPhis.fill(nullptr);
  fcrPhis.fill(nullptr);
  icntPhis.fill(nullptr);
  

  if(bb) {
    ssize_t numInsns = bb->getVecIns().size();
    for(ssize_t i = 0; i < numInsns; i++) {
	const basicBlock::insPair &p = bb->getVecIns()[i];
	rawInsns.push_back(p);
    }
  }
}

void cfgBasicBlock::bindInsns(regionCFG *cfg) {
  if(not(insns.empty())) {
    for(size_t i = 0, n=insns.size(); i < n; i++) {
      delete insns[i];
    }
    insns.clear();
  }

  for(const auto & p : rawInsns) {
    Insn *ins = getInsn(p.first, p.second);
    assert(ins);
    ins->set(cfg,this);
    insns.push_back(ins);
  }
}


void cfgBasicBlock::updateFPRTouched(uint32_t reg, fprUseEnum useType) {
  const uint32_t mask = ~1U;

  //std::cerr << "reg = " << reg << " new use = " << useType 
  //<< " old type = " << fprTouched[reg] << std::endl;
  
  if(useType == fprUseEnum::doublePrec) {
    switch(fprTouched[reg])
      {
      case fprUseEnum::unused:
	fprTouched[(reg & mask)+0] = fprUseEnum::doublePrec;
	switch(fprTouched[(reg & mask)+1])
	  {
	  case fprUseEnum::unused:
	    fprTouched[(reg & mask)+1] = fprUseEnum::doublePrec;
	    break;
	  case fprUseEnum::singlePrec:
	    fprTouched[(reg & mask)+0] = fprUseEnum::both;
	    fprTouched[(reg & mask)+1] = fprUseEnum::both;
	    break;
	  default:
	    break;
	  }
	break;
      case fprUseEnum::singlePrec:
	fprTouched[(reg & mask)+0] = fprUseEnum::both;
	fprTouched[(reg & mask)+1] = fprUseEnum::both;
	break;
      case fprUseEnum::doublePrec:
      case fprUseEnum::both:
	break;
      default:
	die();
      }
  }
  else if(useType == fprUseEnum::singlePrec) {
    switch(fprTouched[reg])
      {
      case fprUseEnum::unused:
	fprTouched[reg] = fprUseEnum::singlePrec;
	break;
      case fprUseEnum::doublePrec:
	fprTouched[(reg & mask)+0] = fprUseEnum::both;
	fprTouched[(reg & mask)+1] = fprUseEnum::both;
	break;
      case fprUseEnum::singlePrec:
      case fprUseEnum::both:
	break;
      default:
	die();
      }
  }
  else {
    die();
  }
}

bool cfgBasicBlock::dominates(const cfgBasicBlock *B) const {
  if(this==B)
    return true;
  if(B->idombb == this)
    return true;
  if(idombb == B)
    return false;
  
  cfgBasicBlock *P = B->idombb;
  while(P) {
    if(P==this)
      return true;
    else if(P->preds.empty())
      break;
    else
      P = P->idombb;
  }
  return false;
}

llvm::BasicBlock *cfgBasicBlock::getSuccLLVMBasicBlock(uint32_t pc) {
  for(cfgBasicBlock *cbb : succs) {
    if(cbb->getEntryAddr() == pc) {
      return cbb->lBB;
    }
  }
  //die();
  return nullptr;
}

void cfgBasicBlock::traverseAndRename(regionCFG *cfg){
  cfg->myIRBuilder->SetInsertPoint(lBB);
  /* this only gets called for the entry block */
  llvmRegTables regTbl(cfg);
  for(size_t i = 0; i < 32; i++) {
    if(cfg->allGprRead[i] or not(cfg->gprDefinitionBlocks[i].empty())) {
      regTbl.loadGPR(i);
    }
  }
  for(size_t i = 0; i < 32; i++) {
    if(cfg->allFprRead[i] or not(cfg->fprDefinitionBlocks[i].empty())) {
      regTbl.loadFPR(i);
    }
  }
  for(size_t i = 0; i < 5; i++) {
    if(cfg->allFcrRead[i] or not(cfg->fcrDefinitionBlocks[i].empty())) {
      regTbl.loadFCR(i);
    }
  }
  if(globals::countInsns) {
    regTbl.initIcnt();
  }

  //lBB->dump();

  termRegTbl.copy(regTbl);
  for(auto nBlock : dtree_succs) {
    /* iterate over instructions */
    cfg->myIRBuilder->CreateBr(nBlock->lBB);
    /* pre-order traversal */
    nBlock->traverseAndRename(cfg, regTbl);
    break;
  }
}

void cfgBasicBlock::patchUpPhiNodes(regionCFG *cfg) {
  for(phiNode* p : phiNodes) {
    for(cfgBasicBlock* bb :  preds) {
      p->addIncomingEdge(cfg, bb);
    }
  }
  
  for(cfgBasicBlock* bb : dtree_succs) {
    bb->patchUpPhiNodes(cfg);
  }
}

void cfgBasicBlock::traverseAndRename(regionCFG *cfg, llvmRegTables prevRegTbl) {
  llvmRegTables regTbl(prevRegTbl);

  /* this gets called for all other blocks block */
  cfg->myIRBuilder->SetInsertPoint(lBB);

  bool innerBlock = false;
  if(cfg->getInnerPerfectBlock()) {
    size_t nestingDepth  = cfg->loopNesting.size();
    innerBlock = cfg->loopNesting[nestingDepth-1][0].inSingleBlockLoop(this);
  }
  if(innerBlock) {
    cfg->getInnerPerfectBlock() = this;
  }


  for(auto p : phiNodes) {
    p->makeLLVMPhi(cfg, regTbl);
  }
  //bb->print();

  if(globals::countInsns) {
    regTbl.incrIcnt(insns.size());
  }

  if(globals::simPoints and insns.size()) {
    if(cfg->builtinFuncts.find("log_bb") != cfg->builtinFuncts.end()) {
      llvm::Value *vAddr = llvm::ConstantInt::get(cfg->type_int32, getEntryAddr());
      std::vector<llvm::Value*> argVector;
      argVector.push_back(vAddr);
      argVector.push_back(regTbl.iCnt);
      llvm::ArrayRef<llvm::Value*> cArgs(argVector);
      cfg->myIRBuilder->CreateCall(cfg->builtinFuncts["log_bb"],cArgs);
    }
  }
  
  /* generate code for each instruction */
  for(size_t i = 0, n=insns.size(); i < n; i++) {
    /* branch delay means we need to skip inst */
    insns[i]->codeGen(this, regTbl);
  }

  termRegTbl.copy(regTbl);
  
  /* walk dominator tree */
  for(auto cbb : dtree_succs){
    /* pre-order traversal */
    cbb->traverseAndRename(cfg, regTbl);
  }
  
  if(not(hasTermBranchOrJump)) {
    uint32_t npc = getExitAddr() + 4;

    llvm::BasicBlock *nBB = 0;
    nBB = getSuccLLVMBasicBlock(npc);
    nBB = cfg->generateAbortBasicBlock(npc, regTbl, this, nBB);

    cfg->myIRBuilder->SetInsertPoint(lBB);
    //print();
    cfg->myIRBuilder->CreateBr(nBB);
    //lBB->dump();
  }

}

uint32_t cfgBasicBlock::getEntryAddr() const {
  if(not(rawInsns.empty())) {
    return rawInsns.at(0).second;
  }
  else if(bb != nullptr) {
    assert(false);
    return bb->getEntryAddr();
  }
  return ~(0U);
}
