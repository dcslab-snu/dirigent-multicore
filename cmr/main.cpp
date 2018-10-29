#include <string.h>
#include "global.h"
#include "timer.h"
#include "process.h"
#include "monitor.h"

sig_atomic_t timer_expired = 0;
sig_atomic_t process_end = 0;
bool use_freq_scale = false;
bool use_partitioning= false;
unsigned int init_partition_num = 2;
bool change_partition=true;

int main(int argc, char *argv[])
{
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0] << " <config-file> <use_freq_scale> <use_partitioning>\n";
    errExit("incorrect inputs");
  }
  char use_freq[] = "freq";
  if (strcmp(use_freq, argv[2]) == 0)
    use_freq_scale = true;

  char use_part[] = "part";
  if (strcmp(use_part, argv[3]) == 0)
    use_partitioning = true;

  if (argc == 5) {
    init_partition_num = atoi(argv[4]);
    change_partition = false;
  }
  else
    init_partition_num = 2;

  process_manager_c mngr(argv[1]);

  m_timer::m_timer_init();
  mngr.init_bak_process();
  mngr.start_bg_process();

  // make sure bg are running
  cerr<<"wait for bg to start"<<endl;
  sleep(1);
  cerr<<"start fg"<<endl;

  mngr.start_fg_process();
  m_timer::m_timer_start();

  do {
    sleep(SLEEP_DURATION);

    if (process_end)
      mngr.restart_process();
    if (timer_expired)
      mngr.timer_expires();
  } while(true);

  return 0;
}

