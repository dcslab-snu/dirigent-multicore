#ifndef __MONITOR_H__
#define __MONITOR_H__

#include <list>
#include <vector>
#include <fstream>
#include <cassert>
#include <float.h>
#include "global.h"
#include "process.h"

#include "cpucounters.h"
#include "msr.h"
#include "utils.h"
#include "types.h"
#include <chrono>

using namespace std;
using namespace chrono;

#define IA32_PCM0                                           (0xC1)
#define IA32_PCM1                                           (0xC2)
#define IA32_PERFEVTSEL0_ADDR                               (0x186)
//#define IA32_PERFEVTSEL0_ADDR                               (0x187)
#define MEM_LOAD_UOPS_LLC_MISS_RETIRED_LOCAL_DRAM_EVTNR     (0xD3)
#define MEM_LOAD_UOPS_LLC_MISS_RETIRED_LOCAL_DRAM_UMASK     (0x01)

struct stat_c {
  uint64 instr[CORE_NUM];
  uint64 l3_miss[CORE_NUM];
  uint64 l3_hit[CORE_NUM];
  uint64 tsc;
  system_clock::time_point tp;
};

struct process_c;
struct process_manager_c;

struct monitor_c {
#ifdef PRINT_SAMPLES
  list<stat_c*>     m_timer_stat;
  ofstream          m_timer_ofile_cycle;
  ofstream          m_timer_ofile_instr;
#endif
  PCM*              m_PCM_handle;
  MsrHandle*        m_MSR_handle[CORE_NUM];
  stat_c*           last_stat;
  process_c**       process;
  int               process_num;
  int               fg_num;
  uint32            sample_num[CORE_NUM];
  uint32            m_sample_num;
  int32             fg_freq_idx[CORE_NUM];
  int32             bg_freq_idx;
  process_manager_c*  mngr;

  void   get_stat(stat_c* s);
  void   perf_mon(stat_c* new_s, stat_c* old_s);
  void   freq_scale();
  void   write_stat(ofstream& fs_cycle, ofstream& fs_instr, stat_c& s, stat_c& e, stat_c& cum);
  void   write_num_samples(ofstream& fs_cycle, ofstream& fs_instr, int count);
  //void   write_num_samples(ofstream& fs_cycle, ofstream& fs_instr);
  //void   write_stat(ofstream& fs, stat_c& s, stat_c& e);
  uint64 get_expect_value(const uint64* rarray, const uint64* sarray, uint64 ins);

  /* public */
  monitor_c(process_c** ps, int pn, int fn, string fg_name, process_manager_c* proc_mngr);
  void  stat_snapshot();
  void  stat_end(int proc_i);
  void  cleanup();
  void  write_timer_stat();
};

#endif
