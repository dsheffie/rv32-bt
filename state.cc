#include <cstring>
#include "state.hh"
#include "disassemble.hh"

std::ostream &operator<<(std::ostream &out, const state_t & s) {
  using namespace std;
  out << "PC : " << hex << s.pc << dec << "\n";
  for(int i = 0; i < 32; i++) {
    out << getGPRName(i) << " : 0x"
	<< hex << s.gpr[i] << dec
	<< "(" << s.gpr[i] << ")\n";
  }
  out << "icnt : " << s.icnt << "\n";
  return out;
}


void initState(state_t *s){

}


