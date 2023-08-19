#ifndef __SIM_BASICBLOCK__
#define __SIM_BASICBLOCK__

#include <set>
#include <vector>
#include <map>
#include <list>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <map>

#include "riscv.hh"
#include "execUnit.hh"

class compile;
class regionCFG;
class basicBlock;

std::ostream &operator<<(std::ostream &out, const basicBlock &bb);

class basicBlock : public execUnit {
public:
  typedef std::pair<uint32_t, uint32_t> insPair;
  typedef std::vector<insPair, backtrace_allocator<insPair>> insContainer;
private:
  friend std::ostream &operator<<(std::ostream &out, const basicBlock &bb);
  friend int main(int, char**);
  friend class compile;
  friend class region;
  friend class regionCFG;
  struct orderBasicBlocks {
    bool operator() (const basicBlock *a, const basicBlock *b) const {
      return a->getEntryAddr() < b->getEntryAddr();
    }
  };
  static uint64_t cfgCnt;
  static std::map<uint32_t, basicBlock*> bbMap;
  static std::map<uint32_t, basicBlock*> insMap;
  static std::map<uint32_t, uint64_t> insInBBCnt;
  uint32_t entryAddr=0;  
  std::set<basicBlock*, orderBasicBlocks> preds,succs;
  std::map<uint32_t, basicBlock *> succsMap;
  bool isCompiled = false, hasRegion = false;
  std::map<uint32_t, uint32_t> bbRegionCounts; 
  std::vector <std::vector<basicBlock*>>bbRegions;
  regionCFG *cfgCplr = nullptr;
  uint32_t termAddr=0;
  bool readOnly=false;
  bool hasjr=false, hasjal=false, hasjalr = false, hasmonitor=false;
  uint64_t totalEdges = 0;
  insContainer vecIns;
  std::map<uint32_t, uint64_t> edgeCnts;
  static bool canCompileRegion(std::vector<basicBlock*> &region);
  /* heads of regions that include this block */
  std::set<basicBlock*> cfgInRegions;
  void toposort(const std::set<basicBlock*> &valid, std::list<basicBlock*> &ordered, std::set<basicBlock*> &visited);
public:
  static void dropAllBBs();
  static void dumpCFG();
  void report(std::string &s, uint64_t icnt) override;
  void info() override;
  basicBlock* run(state_t *s) override;
  static bool validPath(std::vector<basicBlock*> &rpath);
  void addRegion(const std::vector<basicBlock*> &region);
  bool enoughRegions() const;
  basicBlock* split(uint32_t nEntryAddr);
  void setReadOnly();
  void print() const;
  void repairBrokenEdges();
  ssize_t sizeInBytes() const;
  void addIns(uint32_t inst, uint32_t addr);
  basicBlock(uint32_t entryAddr, basicBlock *prev);
  basicBlock(uint32_t entryAddr);
  ~basicBlock();
  void dropCompiledCode();
  basicBlock *findBlock(uint32_t entryAddr);
  /* no mutate */
  static basicBlock *globalFindBlock(uint32_t entryAddr);
  basicBlock *localFindBlock(uint32_t entryAddr);

  bool execute(state_t *s);
  bool executeJIT(state_t *s);
  void addSuccessor(basicBlock *bb);
  bool dfs(basicBlock* oldhead, std::set<basicBlock*> &visited, 
	   std::vector<basicBlock*> &path);
  void setCFG(regionCFG *cfg);
  static size_t numBBs() {
    return bbMap.size();
  }
  static size_t numStaticInsns() {
    return insMap.size();
  }
  uint32_t getEntryAddr() const override{
    return entryAddr; 
  }
  uint32_t getTermAddr() const {
    return termAddr;
  }
  void setTermAddr( uint32_t termAddr) {
    if(this->termAddr == 0) {
      this->termAddr = termAddr;
    }
  }
  const insContainer &getVecIns() const {
    return vecIns;
  }
  double edgeWeight(uint32_t pc) const {
    const auto it = edgeCnts.find(pc);
    if(it == edgeCnts.end()) {
      return  0.0;
    }
    return static_cast<double>(it->second) / (totalEdges==0 ? 1 : totalEdges);
  }
  size_t getNumIns() const {
    return vecIns.size();
  }
  bool blockCompiled() const{
    return isCompiled;
  }
  bool regionCompiled() const{
    return hasRegion;
  }
  bool hasJR(bool isRet=false) const;
  bool hasTermDirectBranchOrJump(uint32_t &target, uint32_t &fallthru) const;
  bool fallsThru() const;
  bool hasJAL() const {
    return hasjal;
  }
  bool hasJALR() const {
    return hasjalr;
  }
  bool hasMONITOR() const {
    return hasmonitor;
  }
  const std::set<basicBlock*, orderBasicBlocks> &getSuccs() const {
    return succs;
  }
  void addToCFGRegions(basicBlock *bb) {
    cfgInRegions.insert(bb);
  }
  bool sanityCheck();
  
  static void toposort(basicBlock *src, const std::set<basicBlock*> &valid, std::list<basicBlock*> &ordered);
};

#define FUNC_STATUS_LIST(x)			\
  x(success)					\
  x(no_return)					\
  x(too_many_returns)				\
  x(recursive_call)				\
  x(monitor)					\
  x(direct_call)				\
  x(indirect_call)				\
  x(arbitrary_jr)  


#define FUNC_STATUS_ENTRY(x) x,
#define FUNC_STATUS_PAIR(x) {funcComplStatus::x, #x},

enum class funcComplStatus {FUNC_STATUS_LIST(FUNC_STATUS_ENTRY)};

funcComplStatus findLeafNodeFunc(basicBlock* entryBB,
				 const std::map<uint32_t, std::pair<std::string, uint32_t>> &syms,
				 std::vector<basicBlock*> &func,
				 int &numErrors);

funcComplStatus findFuncWithInline(basicBlock* entryBB,
				   const std::map<uint32_t, std::pair<std::string, uint32_t>> &syms,
				   const std::set<uint32_t> & leaf_funcs,
				   std::vector<basicBlock*> &func);



inline std::ostream &operator << (std::ostream &out, funcComplStatus s) {
  const static std::map<funcComplStatus, std::string> m = {
    FUNC_STATUS_LIST(FUNC_STATUS_PAIR)
  };
  out <<  m.at(s);
  return out;
}




#undef FUNC_STATUS_LIST
#undef FUNC_STATUS_ENTRY
#undef FUNC_COMPL_STATUS

#endif
