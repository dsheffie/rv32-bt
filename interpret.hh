#ifndef __INTERPRET_HH__
#define __INTERPRET_HH__

#include <cstdint>
static const int MARGS = 20;
struct state_t;

void handle_syscall(state_t *s, uint64_t tohost);

void interpretAndBuildCFG(state_t *s);
void interpret(state_t *s);
void interpretAndBuildCFGEL(state_t *s);
void interpretEL(state_t *s);


#endif
