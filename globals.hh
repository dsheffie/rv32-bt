#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <string.h>
#include <string>
#include <set>
#include <map>

class region;
class basicBlock;
class execUnit;

enum class cfgAugEnum {none, head, aggressive, insane};

namespace globals {
  extern int sArgc;
  extern char** sArgv;
  extern bool isMipsEL;
  extern bool countInsns;
  extern bool simPoints;
  extern bool replay;
  extern uint64_t simPointsSlice;
  extern uint64_t nCfgCompiles;
  extern region *regionFinder;
  extern basicBlock *cBB;
  extern execUnit *currUnit;
  extern bool enClockFuncts;
  extern bool enableCFG;
  extern bool verbose;
  extern bool ipo;
  extern bool fuseCFGs;
  extern uint64_t nFuses;
  extern uint64_t nAttemptedFuses;
  extern bool enableBoth;
  extern uint32_t enoughRegions;
  extern bool dumpIR;
  extern bool splitCFGBBs;
  extern std::string blobName;
  extern uint64_t icountMIPS;
  extern cfgAugEnum cfgAug;
  extern std::string binaryName;
  extern std::set<int> openFileDes;
  extern bool profile;
  extern uint64_t dumpicnt;
  extern uint64_t tohost_addr;
  extern uint64_t fromhost_addr;
  extern std::map<std::string, uint32_t> symtab;
  extern bool log;
#ifndef ELIDE_LLVM
  extern llvm::CodeGenOpt::Level regionOptLevel;
#endif
}

#if ((LLVM_VERSION_MAJOR==3 && LLVM_VERSION_MINOR > 8) || LLVM_VERSION_MAJOR > 3)
#ifndef ELIDE_LLVM
extern llvm::LLVMContext globalContext;
#endif
#endif


#endif
