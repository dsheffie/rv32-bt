/*
 * Adapted by D. Lemire
 * From:
 * Duc Tri Nguyen (CERG GMU)
 * From:
 * Dougall Johnson
 * https://gist.github.com/dougallj/5bafb113492047c865c0c8cfbc930155#file-m1_robsize-c-L390
 *
 */
#include "m1cycles.hh"


#ifdef __APPLE__
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <pthread.h>

#define KPERF_LIST                                                             \
  /*  ret, name, params */                                                     \
  F(int, kpc_get_counting, void)                                               \
  F(int, kpc_force_all_ctrs_set, int)                                          \
  F(int, kpc_set_counting, uint32_t)                                           \
  F(int, kpc_set_thread_counting, uint32_t)                                    \
  F(int, kpc_set_config, uint32_t, void *)                                     \
  F(int, kpc_get_config, uint32_t, void *)                                     \
  F(int, kpc_set_period, uint32_t, void *)                                     \
  F(int, kpc_get_period, uint32_t, void *)                                     \
  F(uint32_t, kpc_get_counter_count, uint32_t)                                 \
  F(uint32_t, kpc_get_config_count, uint32_t)                                  \
  F(int, kperf_sample_get, int *)                                              \
  F(int, kpc_get_thread_counters, int, unsigned int, void *)

#define F(ret, name, ...)                                                      \
  typedef ret name##proc(__VA_ARGS__);                                         \
  static name##proc *name;
KPERF_LIST
#undef F

#define CFGWORD_EL0A32EN_MASK (0x10000)
#define CFGWORD_EL0A64EN_MASK (0x20000)
#define CFGWORD_EL1EN_MASK (0x40000)
#define CFGWORD_EL3EN_MASK (0x80000)
#define CFGWORD_ALLMODES_MASK (0xf0000)

#define CPMU_NONE 0
#define CPMU_CORE_CYCLE 0x02
#define CPMU_INST_A64 0x8c
#define CPMU_INST_BRANCH 0x8d
#define CPMU_SYNC_DC_LOAD_MISS 0xbf
#define CPMU_SYNC_DC_STORE_MISS 0xc0
#define CPMU_SYNC_DTLB_MISS 0xc1
#define CPMU_SYNC_ST_HIT_YNGR_LD 0xc4
#define CPMU_SYNC_BR_ANY_MISP 0xcb
#define CPMU_FED_IC_MISS_DEM 0xd3
#define CPMU_FED_ITLB_MISS 0xd4

#define KPC_CLASS_FIXED (0)
#define KPC_CLASS_CONFIGURABLE (1)
#define KPC_CLASS_POWER (2)
#define KPC_CLASS_RAWPMU (3)
#define KPC_CLASS_FIXED_MASK (1u << KPC_CLASS_FIXED)
#define KPC_CLASS_CONFIGURABLE_MASK (1u << KPC_CLASS_CONFIGURABLE)
#define KPC_CLASS_POWER_MASK (1u << KPC_CLASS_POWER)
#define KPC_CLASS_RAWPMU_MASK (1u << KPC_CLASS_RAWPMU)

#define COUNTERS_COUNT 10
#define CONFIG_COUNT 8
#define KPC_MASK (KPC_CLASS_CONFIGURABLE_MASK | KPC_CLASS_FIXED_MASK)
uint64_t g_counters[COUNTERS_COUNT];
uint64_t g_config[COUNTERS_COUNT];

static void configure_rdtsc() {
  if (kpc_set_config(KPC_MASK, g_config)) {
    printf("kpc_set_config failed\n");
    return;
  }

  if (kpc_force_all_ctrs_set(1)) {
    printf("kpc_force_all_ctrs_set failed\n");
    return;
  }

  if (kpc_set_counting(KPC_MASK)) {
    printf("kpc_set_counting failed\n");
    return;
  }

  if (kpc_set_thread_counting(KPC_MASK)) {
    printf("kpc_set_thread_counting failed\n");
    return;
  }
}

static void init_rdtsc() {
  void *kperf = dlopen(
      "/System/Library/PrivateFrameworks/kperf.framework/Versions/A/kperf",
      RTLD_LAZY);
  if (!kperf) {
    printf("kperf = %p\n", kperf);
    return;
  }
#define F(ret, name, ...)                                                      \
  name = (name##proc *)(dlsym(kperf, #name));                                  \
  if (!name) {                                                                 \
    printf("%s = %p\n", #name, (void *)name);                                  \
    return;                                                                    \
  }
  KPERF_LIST
#undef F

  if (kpc_get_counter_count(KPC_MASK) != COUNTERS_COUNT) {
    printf("wrong fixed counters count\n");
    return;
  }

  if (kpc_get_config_count(KPC_MASK) != CONFIG_COUNT) {
    printf("wrong fixed config count\n");
    return;
  }
  g_config[0] = CPMU_CORE_CYCLE | CFGWORD_EL0A64EN_MASK;
  g_config[3] = CPMU_INST_BRANCH | CFGWORD_EL0A64EN_MASK;
  g_config[4] = CPMU_SYNC_BR_ANY_MISP | CFGWORD_EL0A64EN_MASK;
  g_config[5] = CPMU_INST_A64 | CFGWORD_EL0A64EN_MASK;

  configure_rdtsc();
}

void setup_performance_counters(void) {
  int test_high_perf_cores = 1;
  if (test_high_perf_cores) {
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
  } else {
    pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
  }
  init_rdtsc();
  configure_rdtsc();
}

void stop_performance_counters(void) {

}

performance_counters get_counters(void) {
  if (kpc_get_thread_counters(0, COUNTERS_COUNT, g_counters)) {
    return 1;
  }
  // g_counters[3 + 2] gives you the number of instructions 'decoded'
  // whereas g_counters[1] might give you the number of instructions 'retired'.
  return performance_counters{g_counters[0 + 2], g_counters[3 + 2],
                              g_counters[4 + 2], g_counters[5 + 2]};
}

#elif __linux__
#define __GNU_SOURCE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <cassert>
#include <cstdint>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sched.h>
#include <sys/ioctl.h>

static int perf_fds[4] = {-1};

void setup_performance_counters(void) {
  struct perf_event_attr pe;
  memset(&pe, 0, sizeof(pe));
  pe.type = PERF_TYPE_HARDWARE;
  pe.size = sizeof(pe);
  pe.disabled = 0;
  int cpu = sched_getcpu();
  pe.config = PERF_COUNT_HW_CPU_CYCLES;  
  perf_fds[0] = syscall(__NR_perf_event_open, &pe, 0, cpu, -1, 0);
  if(perf_fds[0] < 0) perror("WTF");

  pe.config = PERF_COUNT_HW_BRANCH_INSTRUCTIONS;
  perf_fds[1] = syscall(__NR_perf_event_open, &pe, 0, cpu, -1, 0);
  if(perf_fds[1] < 0) perror("WTF");

  pe.config = PERF_COUNT_HW_BRANCH_MISSES;
  perf_fds[2] = syscall(__NR_perf_event_open, &pe, 0, cpu, -1, 0);
  if(perf_fds[2] < 0) perror("WTF");  
  
  pe.config = PERF_COUNT_HW_INSTRUCTIONS;
  perf_fds[3] = syscall(__NR_perf_event_open, &pe, 0, cpu, -1, 0);
  if(perf_fds[3] < 0) perror("WTF");

}

void stop_performance_counters(void) {
  for(int i = 0; i < 4; i++) {
    if(perf_fds[i] != -1) {
      close(perf_fds[i]);
    }
  }
}

performance_counters get_counters(void) {
  uint64_t cycles, inst;
  ssize_t rc;
  rc = read(perf_fds[0], &cycles, sizeof(cycles));
  assert(rc == sizeof(cycles));
  rc = read(perf_fds[3], &inst, sizeof(inst));
  return performance_counters{cycles,0UL,0UL,inst};
}

#endif
