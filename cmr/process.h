#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <stdint.h>
#include <signal.h>
#include <string>
#include <iostream>
#include "global.h"
#include <list>
#include "monitor.h"
#include <fcntl.h>
#include "types.h"
#include "cpucounters.h"
#include <chrono>

using namespace std;
using namespace chrono;


#define IA32_L3_MASK               0xC90
#define IA32_L3_MASK_NUM           4
//#define IA32_PQR_ASSOC             0xC8F
#define IA32_PQR_ASSOC_QECOS_SHIFT (32)
#define IA32_PQR_ASSOC_QECOS_MASK  0xffffffff00000000ULL

extern sig_atomic_t process_end;
extern sig_atomic_t timer_expired;

struct process_c {
  const char* name;
  int       name_len;           // length of name excluding NULL
  uint32    core_id;            // which core to run on
  uint32    core_ids[CORE_NUM]; // which core ids to run on
  int       ci_count;           // the number of cores

  int       iter;               // how many iteration has run
  int       total_iter;         // how many iteration to run in total
  bool      fg;                 // if it is a fg job
  char*     cmd[MAX_ARGV_LEN];  // cmd to run the process
  int       cmd_len;            // number of arguments excluding NULL
  bool      done;               // if has finished
  bool      stop;
  pid_t     pid;                // pid in the host system
  int       fd;                 // fd for redirected stdout
  char*     start_tsc_ptr;      // start time of child process
  char*     exec_cyc_ptr;       // execution time of child process

  float     avg_l3_miss;        // average number of L3 misses
  uint64    total_l3_miss;
  uint64    my_l3_miss;
  uint64    my_l3_hit;
  uint64    total_instr;        // number of instructions executed
  int64     cumulative_penalty;
  float     eta_target;         // eta_target : estimated time of arrival of target

  uint64*   fg_op_freq;         // fg's operation time at each freq.
  uint64*   bg_op_freq;         // bg's operation time at each freq.
  uint64    bg_stop;            // bg's stop time
  uint64    bg_instr;
  uint64    total_overhead;
  double    energy;
  int       sample_num;

  float     eta_avg;            // eta_avg : avg. estimated time of arrival
  int64     real_cyc;           // cumulative execution cycles of a process

  SocketCounterState* last_skstate;

  uint64*    exec_record;       // history of instr.  (over multiple executions) TODO: update the fraction of process (instr.)
  uint64*    l3_record;         // history of l3 miss (over multiple executions) TODO: update the fraction of l3miss
  uint32     head;              // index of exec_record and l3_record TODO: Head is updated at every process end, we need to change the location of update
  float*     corr;              // pointer of correlation table (each elem : corr when partition varies)
  float*     hit_rate;          // pointer of hit_rate (over multiple executions) (each elem : hit_rate when partition varies)

  FILE*     cpu_speed_files[CORE_NUM];  // FIXME: Modified `cpu_speed_file` to `cpu_speed_files` to support multi-threads
  uint64    start_tsc;          // start tsc. of each process
  system_clock::time_point    exec_start_time;      // process start time measured by "chrono"
  system_clock::time_point    exec_end_time;        // process end time measured by "chrono"
  duration<double>            elapsed_time;

  int partition_num;
};

struct monitor_c;

struct process_manager_c {
  monitor_c* m_monitor;
  process_c* m_process[MAX_PROCESS_NUM];
  process_c* m_process_bak[MAX_PROCESS_NUM];
  int     process_num;
  int     fg_num;
  int     bak_process_num;

  float   bg_slow_ratio;
  float   fg_slow_ratio;
  int     partition_num;
  uint64  fg_mask;
  SocketCounterState* last_skstate;

  process_manager_c( char* filename);
  void    append_cmd(process_c* p, string s);

  void    start_process(process_c* p);
  void    start_bg_process();
  void    start_fg_process();
  void    init_bak_process();
  void    switch_bg_process();
  void    restart_process();
  void    terminate_process();
  void    timer_expires();
  void    check_partition();
  void    init_partition();
  void    repartition();
};

float compute_corr(uint64* x, uint64* y);
#endif
