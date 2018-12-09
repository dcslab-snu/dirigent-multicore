#include "monitor.h"
#include <sys/wait.h>
#include <sys/mman.h>
#include <assert.h>
#include <math.h>

using namespace std;
using namespace chrono;

float compute_corr(uint64 *x, uint64 *y) {
    float s_x = 0;
    float s_y = 0;
    float s_xx = 0;
    float s_yy = 0;
    float s_xy = 0;
    for (int i = 0; i < EXEC_RECORD_LEN; i++) {
        s_x += x[i];
        s_y += y[i];
        s_xx += (float) x[i] * x[i];
        s_yy += (float) y[i] * y[i];
        s_xy += (float) x[i] * y[i];
    }
    float up = s_xy * EXEC_RECORD_LEN - s_x * s_y;
    float down1 = sqrt(EXEC_RECORD_LEN * s_xx - s_x * s_x);
    float down2 = sqrt(EXEC_RECORD_LEN * s_yy - s_y * s_y);
    return up / down1 / down2;
}

void child_handler(int sig, siginfo_t *si, void *uc) {
    process_end = 1;
}

process_manager_c::process_manager_c(char *filename) {
    struct sigaction sa;
    srand(0);
    bg_slow_ratio = 0;

    /* register SIGCHLD handler */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = child_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
        errExit("sigaction");

    for (int i = 0; i < MAX_PROCESS_NUM; i++) {
        m_process[i] = NULL;
        m_process_bak[i] = NULL;
    }

    /* open config file and populate process array */
    ifstream ifile(filename, ifstream::in);
    if (!ifile.is_open()) {
        errExit("open config file");
    }

    /* get total number of processes */
    process_num = 0;
    fg_num = 0;
    bak_process_num = 0;

    int total_process_num;
    ifile >> total_process_num;
    if (total_process_num > MAX_PROCESS_NUM) {
        errExit("too many processes");
    }

    // get the number of FG and BG processes
    /*int fg_process_num;
    int bg_process_num;

    ifile >> fg_process_num;
    if (fg_process_num > MAX_PROCESS_NUM) {
      errExit("too many processes");
    }

    ifile >> bg_process_num;
    if (bg_process_num > MAX_PROCESS_NUM) {
      errExit("too many processes");
    }

    string* fg_names = new string[fg_process_num];
    for (int i=0; i<fg_process_num; i++){
      ifile >> fg_names[i];
    }
    string* bg_names = new string[bg_process_num];
    for (int i=0; i<bg_process_num; i++){
      ifile >> bg_names[i];
    }*/
    /* get FG name */
    string fg_workload_name;
    ifile >> fg_workload_name;
    fg_name = fg_workload_name;

    string bg_workload_name;
    ifile >> bg_workload_name;
    bg_name = bg_workload_name;


    /* input file format:
     * core_id total_iter cmd
     * core_id: which core to run on     # core_ids(e.g., 0,2,2 or 8-15)
     * total_iter: number of iterations (0 indicates background job)
     * cmd: command to run the process
     * NULL: end of parameters
     */

    string core_ids_del = ",";

    for (int i = 0; i < total_process_num * 2 - 1; i++) {
        process_c *t_proc = new process_c;
        string core_ids_str;
        //ifile >> t_proc->core_id;
        ifile >> core_ids_str;
        int str_len = core_ids_str.size();
        cout << "core_ids_str[" << str_len - 1 << "] : ";
        if (str_len > 0) {
            cout << core_ids_str[str_len - 1] << endl;
        }
        else {
            cout << "None" << endl;
        }
        if (core_ids_str[str_len - 1] != ',') {
            cout << "some cpu affinity settings are not ended with comma (e.g., 0,)" << endl;
        }
        // TODO: t_proc->core_id should be changed to core_ids (in .cfg file, cpu affinity must end with comma)
        // e.g., 0,1,2,3,4,5,6,7,
        size_t pos = 0;
        int ci_count = 0;
        string token;
        while ((pos = core_ids_str.find(core_ids_del)) != string::npos) {
            token = core_ids_str.substr(0, pos);
            cout << "[while] token: " << token << endl;
            cout << "[while] ci_count: " << ci_count << endl;
            t_proc->core_ids[ci_count] = stoi(token);
            cout << "[while] t_proc->core_ids: " << t_proc->core_ids[ci_count] << endl;
            core_ids_str.erase(0, pos + core_ids_del.length());
            ci_count++;
        }
        t_proc->ci_count = ci_count;
        if (ifile.eof())
            break;
        ifile >> t_proc->total_iter;
        if (t_proc->total_iter != 0)
            ifile >> t_proc->eta_target;
        else
            t_proc->eta_target = 0;

        // FIXME: t_proc->core_id should be changed to core_ids
        for (int j = 0; j < t_proc->ci_count; j++) {
            if (t_proc->core_ids[j] > CORE_NUM) {
                errExit("invalid core id");
            }
        }

        /*
        string cpu_freq_fn = "/sys/devices/system/cpu/cpu";
        cpu_freq_fn += '0' + t_proc->core_id;
        // TODO: t_proc->core_id should be changed to core_ids -> for loop for core_id
        cpu_freq_fn += "/cpufreq/scaling_setspeed";
        t_proc->cpu_speed_file = fopen(cpu_freq_fn.c_str(), "w");
        */

        // FIXME: t_proc->core_id should be changed to core_ids -> for loop for core_id
        for (int j = 0; j < t_proc->ci_count; j++) {
            string cpu_freq_fn = "/sys/devices/system/cpu/cpu";
            cpu_freq_fn += to_string(t_proc->core_ids[j]);
            cpu_freq_fn += "/cpufreq/scaling_setspeed";
            t_proc->cpu_speed_files[j] = fopen(cpu_freq_fn.c_str(), "w");
        }

        if (t_proc->total_iter != 0) {
            /* initialize shared memory for inter-process communication */
            char filename[] = "/myshm0";
            filename[6] += fg_num;
            cerr << "Shared Memory:" << filename << endl;
            int fd = shm_open(filename, O_RDWR | O_CREAT, 0666);
            assert(fd != -1);
            int ret = ftruncate(fd, SM_SIZE); // enough size for all FGs
            assert(ret != -1);
            char *file_memory = (char *) mmap(NULL, SM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
            assert(file_memory != (void *) -1);
            cerr << "mmap: passed" << endl;
            t_proc->fg = true;
            //get fg_name
            /*t_proc->name_len = fg_names[process_num].length();
            t_proc->name = fg_names[process_num].c_str();*/

            t_proc->iter = 0;
            t_proc->done = false;
            t_proc->start_tsc_ptr = file_memory;
            t_proc->exec_cyc_ptr = file_memory + 100;
            t_proc->fg_op_freq = new uint64[MAX_FREQ_IDX + 1];
            t_proc->bg_op_freq = new uint64[MAX_FREQ_IDX + 1];

            t_proc->exec_record = new uint64[EXEC_RECORD_LEN];
            t_proc->l3_record = new uint64[EXEC_RECORD_LEN];
            t_proc->corr = new float[MAX_PARTITION_NUM];
            t_proc->hit_rate = new float[MAX_PARTITION_NUM];
            for (int j = 0; j < MAX_PARTITION_NUM; j++) {
                t_proc->corr[j] = 0.5;
                t_proc->hit_rate[j] = 0.5;
            }
            t_proc->head = 0;

            for (int j = 0; j <= MAX_FREQ_IDX; j++) {
                t_proc->fg_op_freq[j] = (uint64) 0;
                t_proc->bg_op_freq[j] = (uint64) 0;
            }
            /* initialize socket power state */
            //t_proc->last_skstate = new SocketCounterState;
            //*(t_proc->last_skstate) = getSocketCounterState(0);
            t_proc->last_skstate = NULL;
            sprintf(t_proc->start_tsc_ptr, "%llu", (unsigned long long) 0);
            sprintf(t_proc->exec_cyc_ptr, "%llu", (unsigned long long) 0);
            fg_num++;
        }
        else {
            /*t_proc->name_len = bg_names[bak_process_num].length();
            t_proc->name = bg_names[bak_process_num].c_str();*/

            t_proc->done = true;
            t_proc->start_tsc_ptr = NULL;
            t_proc->exec_cyc_ptr = NULL;
            t_proc->fg_op_freq = NULL;
            t_proc->bg_op_freq = NULL;
        }
        t_proc->avg_l3_miss = 0;
        t_proc->my_l3_miss = 0;
        t_proc->my_l3_hit = 0;
        t_proc->total_l3_miss = 0;

        string m_cmd;
        append_cmd(t_proc, "/usr/bin/taskset");
        append_cmd(t_proc, "-c");
        // TODO: t_proc->core_id should be changed to core_ids -> for loop for core_id
        //m_cmd = '0' + t_proc->core_id;
        cout << "t_proc->ci_count: " << t_proc->ci_count << endl;
        for (int j = 0; j < t_proc->ci_count; j++) {
            cout << "t_proc->core_ids[" << j << "]: " << t_proc->core_ids[j] << endl;
            if (j == t_proc->ci_count - 1) { m_cmd = m_cmd + to_string(t_proc->core_ids[j]); }
            else {
                m_cmd = m_cmd + to_string(t_proc->core_ids[j]) + ",";
            }
        }
        append_cmd(t_proc, m_cmd);
        append_cmd(t_proc, "/usr/bin/nice");
        append_cmd(t_proc, "-n");
        if (t_proc->fg)
            append_cmd(t_proc, "-15");
        else
            append_cmd(t_proc, "-5");

        while (ifile.peek() != '\n') {
            ifile >> m_cmd;
            if (m_cmd == "native" || m_cmd == "simmedium") {
                input_size = m_cmd;
            }
            append_cmd(t_proc, m_cmd);
        }
        int j = t_proc->cmd_len;
        t_proc->cmd[j] = NULL;
        cerr << "append_cmd: passed" << endl;
        /* check if this is the first process to run on a core */
        bool handled = false;
        for (int j = 0; j < MAX_PROCESS_NUM; j++) {
            // FIXME: t_proc->core_id should be changed to core_ids -> for loop for core_id
            int count = 0;
            int out = 0;
            int overlapped = 0;
            cout << "m_process[" << j << "]: " << m_process[j] << endl;
            //cout<<"m_process["<<j<<"]->ci_count: " << m_process[j]->ci_count<< endl;
            if (m_process[j] != NULL) {
                // check if t_proc is the m_process[j] which is bak_ground process
                //cerr<<"m_process["<<j<<"]->ci_count:" << m_process[j]->ci_count<< endl;
                for (int k = 0; k < m_process[j]->ci_count; k++) {
                    // if any core id do not match to t_proc core ids, then break!
                    cout << "[count loop] m_process[" << j << "]->core_ids[" << k << "]: " << m_process[j]->core_ids[k];
                    cout << ", ci_count : " << m_process[j]->ci_count << endl;
                    cout << "[count loop] t_proc->core_ids[" << k << "]: " << t_proc->core_ids[k];
                    cout << ", ci_count : " << t_proc->ci_count << endl;
                    if (m_process[j]->core_ids[k] == t_proc->core_ids[k]) {
                        count++;
                        cout << "count:" << count << " ,m_process[" << j << "]->ci_count: " << m_process[j]->ci_count
                             << endl;
                        if (m_process[j]->ci_count - 1 == count) {
                            overlapped = 1;
                        }
                    }
                }
                // Comparing the t_proc's core counts with m_process[j]->ci_count...
                // The below code finds whether the t_proc is overlapped with m_process[j]..
                if (count == m_process[j]->ci_count - 1 && overlapped) {
                    m_process_bak[bak_process_num] = t_proc;
                    bak_process_num++;
                    handled = true;
                    out = 1;
                }
                if (out == 1)
                    break;
            }
        }
        if (!handled) {
            m_process[process_num] = t_proc;
            process_num++;
        }
        cout << "process_num : " << process_num << endl;
    }

    /* bak process number should be the same as process number
     * to ensure 1:1 relation between proc and bak proc */
    cout << "bak_process_num : " << bak_process_num << endl;
    cout << "fg_num : " << fg_num << endl;
    cout << "process_num : " << process_num << endl;
    if (bak_process_num != 0)
        assert(process_num == bak_process_num + fg_num);

    ifile.close();
    cerr << "before monitor_c: passed" << endl;
    m_monitor = new monitor_c(m_process, process_num, fg_num, fg_workload_name, this);
    cerr << "after monitor_c: passed" << endl;
    if (use_partitioning)
        init_partition();

    for (int i = 0; i < process_num; i++) {
        if (m_process[i] != NULL) {
            // FIXME: t_proc->core_id should be changed to core_ids -> for loop for core_id
            for (int j = 0; j < m_process[i]->ci_count; j++) {
                if (j == 0) {
                    fprintf(stderr, "core: ");
                    if (j == m_process[i]->ci_count - 1) {
                        fprintf(stderr, "%d\n", m_process[i]->core_ids[j]);
                    }
                }
                if (j == m_process[i]->ci_count - 1) {
                    fprintf(stderr, "%d\n", m_process[i]->core_ids[j]);
                }
                else {
                    fprintf(stderr, "%d, ", m_process[i]->core_ids[j]);
                }
            }
            fprintf(stderr, "command: ");
            for (int j = 0; j < m_process[i]->cmd_len; j++)
                fprintf(stderr, "%s ", m_process[i]->cmd[j]);
            fprintf(stderr, "\n");
            if (m_process[i]->fg)
                fprintf(stderr, "iteration: %d\n", m_process[i]->total_iter);
        }
    }

    for (int i = 0; i < bak_process_num; i++) {
        if (m_process_bak[i] != NULL) {
            // FIXME: t_proc->core_id should be changed to core_ids -> for loop for core_id
            for (int j = 0; j < m_process_bak[i]->ci_count; j++) {
                if (j == 0) {
                    fprintf(stderr, "core: ");
                    if (j == m_process_bak[i]->ci_count - 1) {
                        fprintf(stderr, "%d\n", m_process_bak[i]->core_ids[j]);
                    }
                }
                if (j == m_process_bak[i]->ci_count - 1) {
                    fprintf(stderr, "%d\n", m_process_bak[i]->core_ids[j]);
                }
                else {
                    fprintf(stderr, "%d, ", m_process_bak[i]->core_ids[j]);
                }
            }
            fprintf(stderr, "command: ");
            for (int j = 0; j < m_process_bak[i]->cmd_len; j++)
                fprintf(stderr, "%s ", m_process_bak[i]->cmd[j]);
            fprintf(stderr, "\n");
            if (m_process_bak[i]->fg)
                fprintf(stderr, "iteration: %d\n", m_process_bak[i]->total_iter);
        }
    }

}

void process_manager_c::append_cmd(process_c *p, string s) {
    int j = p->cmd_len;
    p->cmd[j] = new char[s.length() + 1];
    size_t size = s.copy(p->cmd[j], MAX_ARGV_LEN);
    p->cmd[j][size] = '\0';
    p->cmd_len++;
}

void process_manager_c::start_bg_process() {
    for (int i = 0; i < process_num; i++) {
        if (!m_process[i]->fg)
            start_process(m_process[i]);
    }
}

void process_manager_c::start_fg_process() {
    for (int i = 0; i < process_num; i++) {
        if (m_process[i]->fg) {
            start_process(m_process[i]);
        }
    }
}

void process_manager_c::init_bak_process() {
    if (bak_process_num == 0)
        return;
    for (int i = 0; i < bak_process_num; i++) {
        start_process(m_process_bak[i]);
        kill(m_process_bak[i]->pid, SIGSTOP);
    }
}

void process_manager_c::start_process(process_c *p) {
    if (p == NULL)
        errExit("process not inited");
    p->start_tsc = (uint64) 0;
    p->cumulative_penalty = (uint64) 0;
    p->stop = false;
    p->eta_avg = 0;
    p->real_cyc = 0;
    p->elapsed_time = (duration<double>) 0;

    p->total_instr = 0;
    p->bg_instr = 0;
    p->bg_stop = 0;
    p->total_overhead = 0;

    p->energy = 0;
    p->avg_l3_miss = 0;
    p->my_l3_miss = 0;
    p->my_l3_hit = 0;
    p->total_l3_miss = 0;

    if (p->fg) {
        for (int j = 0; j <= MAX_FREQ_IDX; j++) {
            p->fg_op_freq[j] = 0;
            p->bg_op_freq[j] = 0;
        }
    }

    if (p->last_skstate != NULL) {
        delete p->last_skstate;
        p->last_skstate = NULL;
    }

    pid_t pid = fork();
    if (pid == 0) { /* child */
        /// Redirect Output to STDOUT
        /* create file for stdout */
        // TODO: t_proc->core_id should be changed to core_ids -> for loop for core_id
        string filename;
        string base_path = "../logs/" + input_size + "/";
        //string filename_tmp = "proc_";
        string filename_tmp = fg_name + "_" + bg_name + ".log";
        /*
        string filename_tmp = "proc_";
        string affinity;
        for (int i=0; i<p->ci_count; i++){
          if (i==p->ci_count-1) {affinity = affinity + to_string(p->core_ids[i]);}
          else{
            affinity = affinity + to_string(p->core_ids[i]) + ",";
          }
        }
        filename = filename_tmp + affinity + ".out";
        */
        filename = base_path + filename_tmp;
        p->fd = open(filename.c_str(),
                     O_CREAT | O_APPEND | O_WRONLY, 0644);
        if (p->fd < 0)
            errExit("open file for stdout");

        /* redirect stdout and stderr to newfd */
        dup2(p->fd, STDOUT_FILENO);
        dup2(p->fd, STDERR_FILENO);

        ///

        // FIXME: Measuring TSC values and Initialize Execution times (in seconds)
        m_monitor->m_MSR_handle[0]->read(16, &p->start_tsc);
        p->exec_start_time = system_clock::now();

        /* switch context */
        int ret = execv(p->cmd[0], p->cmd);
        // FIXME: Add the logic for updating start_tsc of child process (start time of child process)
        //uint64 start_tsc;
        //m_monitor->m_MSR_handle[0]->read(16, &start_tsc);
        //sprintf(p->start_tsc_ptr, "%llu", start_tsc);
        //m_monitor->m_MSR_handle[0]->read(16, &p->start_tsc_ptr);
        if (ret == -1)
            errExit("exec returned");
    }
    else if (pid == -1) {
        errExit("error creating process");
    }
    else { /* parent */
        if (p->iter > p->total_iter) {
            p->done = true;
        }
        p->pid = pid;
        p->iter++;
    }
}

void process_manager_c::switch_bg_process() {
    if (bak_process_num == 0) return;

    process_c *p;
    int bi = 0;
    for (int i = 0; i < process_num; i++) {
        if (m_process[i]->fg) continue;
        if (m_process[i]->stop == true) continue;
        if (rand() % 100 <= 49) continue;

        kill(m_process[i]->pid, SIGSTOP);
        kill(m_process_bak[bi]->pid, SIGCONT);

        p = m_process[i];
        m_process[i] = m_process_bak[bi];
        m_process_bak[bi] = p;
        bi++;
        assert(bi <= bak_process_num);
    }
}

void process_manager_c::timer_expires() {
    m_monitor->stat_snapshot();
    timer_expired = 0;
}

void process_manager_c::restart_process() {
    int status;
    bool terminate = true;
    bool have_switched = false;

    for (int i = 0; i < process_num; i++) {
        pid_t ret = waitpid(m_process[i]->pid, &status, WNOHANG);
        if (ret == -1) {
            cerr << "ERROR WAITPID " << m_process[i]->pid << endl;
        }
        if (ret != 0) {
            m_monitor->stat_end(i);
            if (m_process[i]->fg) {
                //cout<<","<<m_monitor->sample_num[m_process[i]->core_id]<<","; // NOT USED
                //cout<<m_process[i]->core_id<<",";
                cout << m_process[i]->core_ids[0] << ","; //Modified core_id to core_ids[0]
                // TODO: t_proc->core_id should be changed to core_ids -> for loop for core_id

                unsigned long long exec_cyc;
                unsigned long long start_tsc;
                //FIXME: Adding program execution duration
                //nanoseconds elapsed_nsecs = m_process[i]->exec_end_time - m_process[i]->exec_start_time;
                // TODO: showing elapsed time in seconds
                //cout << m_process[i]->elapsed_time.count()/1000000000.0 << ",";

                //FIXME: real_cyc means the total cycles of process

                string exec_cyc_str = to_string(m_process[i]->real_cyc);
                const char *real_cyc_str = exec_cyc_str.c_str();
                sscanf(real_cyc_str, "%llu", &exec_cyc);
                //cout<<exec_cyc<<",";

                string s_tsc_str = to_string(m_process[i]->start_tsc);
                const char *start_tsc_str = s_tsc_str.c_str();
                sscanf(start_tsc_str, "%llu", &start_tsc);
                //cout<<start_tsc<<",";

#ifdef PRINT_SAMPLES
                //unsigned long long exec_cyc;
                //sscanf(m_process[i]->exec_cyc_ptr, "%llu", &exec_cyc);
                //cout<<exec_cyc<<",";
                //cout<<m_process[i]->start_tsc<<",";
                m_monitor->write_timer_stat();
#endif

                cout << m_process[i]->eta_avg << ",";

                cout << m_process[i]->bg_instr << ",";
                for (int j = 0; j <= MAX_FREQ_IDX; j++) {
                    cout << m_process[i]->fg_op_freq[j] << ",";
                }
                cout << ",";
                for (int j = 0; j <= MAX_FREQ_IDX; j++) {
                    cout << m_process[i]->bg_op_freq[j] << ",";
                }
                cout << m_process[i]->bg_stop << ",";
                cout << m_process[i]->total_overhead << ",";
                //cout<<m_process[i]->energy<<",";
                cout << m_process[i]->total_instr << ",";
                cout << m_process[i]->my_l3_miss << ",";
                cout << m_process[i]->my_l3_hit << ",";
                cout << m_process[i]->total_l3_miss << ",";
                cout << partition_num << ",";
                if (use_partitioning) {
                    cout << m_process[i]->corr[partition_num] << ",";
                    cout << m_process[i]->hit_rate[partition_num] << ",";
                }
                /* TODO: Commenting below codes to compare dirigent with hybridiso
                if (use_partitioning && change_partition) {
                  // update execution record
                  int head = m_process[i]->head%EXEC_RECORD_LEN;
                  m_process[i]->exec_record[head] = exec_cyc;
                  m_process[i]->l3_record[head] = m_process[i]->my_l3_miss;
                  float corr_tmp = compute_corr(m_process[i]->exec_record, m_process[i]->l3_record);
                  float hit_rate_tmp = (float)m_process[i]->my_l3_hit / (m_process[i]->my_l3_hit + m_process[i]->my_l3_miss);
                  cout<<corr_tmp<<",";
                  cout<<hit_rate_tmp<<",";
                  // if there are enough records
                  if (m_process[i]->head+1 >= EXEC_RECORD_LEN) {
                    m_process[i]->corr[partition_num] = corr_tmp;
                    m_process[i]->hit_rate[partition_num] = m_process[i]->hit_rate[partition_num]*0.5 + hit_rate_tmp*0.5;
                    if (i == 0) // assume process 0 will always bg FG
                      check_partition();
                  }
                  m_process[i]->head ++;
                }
                */
                cout << endl;

                // assume all FGs don't share cores
                //m_monitor->sample_num[m_process[i]->core_id] = 0;
                // TODO: t_proc->core_id should be changed to core_ids -> for loop for core_id
                m_monitor->sample_num[m_process[i]->core_ids[0]] = 0;
                fflush(stdout);
                sprintf(m_process[i]->start_tsc_ptr, "%llu", (unsigned long long) 0);
                sprintf(m_process[i]->exec_cyc_ptr, "%llu", (unsigned long long) 0);
            }
            start_process(m_process[i]);
            if (!have_switched) {
                switch_bg_process();
                have_switched = true;
            }
        }
        terminate &= m_process[i]->done;
    }

    if (terminate) {
        /* Block terminate and timeout signal */
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigaddset(&mask, SIGRTMIN);
        if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
            errExit("sigprocmask");

        terminate_process();
    }
    process_end = 0;
}

void process_manager_c::terminate_process() {
    cerr << "terminating processes" << endl;
    fflush(stdout);
    for (int i = 0; i < process_num; i++) {
        //kill(m_process[i]->pid, SIGKILL);
        if (kill(m_process[i]->pid, SIGKILL) == -1)
            errExit("kill process");
        // FIXME: changed to support multiple cores
        for (int j = 0; j < m_process[i]->ci_count; j++) {
            fclose(m_process[i]->cpu_speed_files[j]);
        }
        //fclose(m_process[i]->cpu_speed_file);
        //m_process[i]->exec_end_time = system_clock::now();
        //cout << system_clock::to_time_t(m_process[i]->exec_end_time) << endl;
    }
    if (bak_process_num != 0) {
        for (int i = 0; i < bak_process_num; i++) {
            if (kill(m_process_bak[i]->pid, SIGKILL) == -1)
                errExit("kill process");
            // FIXME: changed to support multiple cores
            for (int j = 0; j < m_process_bak[i]->ci_count; j++) {
                fclose(m_process_bak[i]->cpu_speed_files[j]);
            }
            //fclose(m_process_bak[i]->cpu_speed_file);
        }
    }
    m_monitor->cleanup();
    shm_unlink("/myshm");
    exit(EXIT_SUCCESS);
}

// assuming all FGs will behave the same way in the long run
// only check the FG on core 0
void process_manager_c::check_partition() {
    float bg_slow = m_process[0]->bg_op_freq[0] + m_process[0]->bg_stop;
    float fg_slow = m_process[0]->fg_op_freq[0];
    float bg_total = m_process[0]->bg_stop;
    float fg_total = 0;
    for (int i = MIN_FREQ_IDX; i <= MAX_FREQ_IDX; i++) {
        bg_total += m_process[0]->bg_op_freq[i];
        fg_total += m_process[0]->fg_op_freq[i];
    }

    bg_slow_ratio = bg_slow_ratio * 0.6 + bg_slow / bg_total * 0.4;
    fg_slow_ratio = fg_slow_ratio * 0.6 + fg_slow / fg_total * 0.4;

    bool slow = 0;
    for (int i = 0; i <= EXEC_RECORD_LEN; i++) {
        //cout << "m_process[0]->exec_record["<<i<<"] :" <<m_process[0]->exec_record[i] << endl;
        if (m_process[0]->exec_record[i] > m_process[0]->eta_target * 1.1) {
            slow = 1;
            break;
        }
    }

    bool hit_improve = false;
    if (partition_num > MIN_PARTITION_NUM) {
        hit_improve = ((m_process[0]->hit_rate[partition_num] - m_process[0]->hit_rate[partition_num - 1]) > 0);
    }

    if (partition_num + 1 < MAX_PARTITION_NUM && m_process[0]->corr[partition_num] > 0.75 && slow &&
        fg_slow_ratio < 0.25) {
        m_process[0]->corr[partition_num] = 0.5;
        partition_num += 1;
        repartition();
        m_process[0]->head = -1;
        return;
    }
    if (partition_num + 1 < MAX_PARTITION_NUM && bg_slow_ratio > 0.5 && fg_slow_ratio < 0.1) {
        m_process[0]->corr[partition_num] = 0.5;
        partition_num += 1;
        repartition();
        m_process[0]->head = -1;
        return;
    }
    if (m_process[0]->corr[partition_num] < 0.25 && partition_num > MIN_PARTITION_NUM &&
        (!hit_improve || fg_slow_ratio > 0.9)
        && !slow) {
        //&& bg_slow_ratio < 0.1 && !slow) {
        m_process[0]->corr[partition_num] = 0.5;
        partition_num = partition_num - 1;
        repartition();
        m_process[0]->head = -1;
        return;
    }
}

void process_manager_c::init_partition() {
    // uses two partition classes
    // 0 for FG, initialized to 0x00003
    // 1 for BG, initialized to 0xffffc

    // init partition sizes
    partition_num = init_partition_num;
    repartition();

    // associate each core with the corresponding cos
    uint64 cos0 = ((uint64) 0 << IA32_PQR_ASSOC_QECOS_SHIFT) & IA32_PQR_ASSOC_QECOS_MASK;
    uint64 cos1 = ((uint64) 1 << IA32_PQR_ASSOC_QECOS_SHIFT) & IA32_PQR_ASSOC_QECOS_MASK;
    for (int i = 0; i < process_num; i++) {
        int ci = m_process[i]->core_id;
        /*if (m_process[i]->fg)
          m_monitor->m_MSR_handle[ci]->write(IA32_PQR_ASSOC, cos0);
        else
          m_monitor->m_MSR_handle[ci]->write(IA32_PQR_ASSOC, cos1);
        */
        // FIXME: core_id is temporarily changed to core_ids[0]
        //int ci = m_process[i]->core_ids[0];
        // FIXME: hard coded fg:0~7, bg:8~15
        //if (m_process[i]->fg)
        if (i < 8)
            m_monitor->m_MSR_handle[i]->write(IA32_PQR_ASSOC, cos0);
        else
            m_monitor->m_MSR_handle[i]->write(IA32_PQR_ASSOC, cos1);
    }
}

void process_manager_c::repartition() {
    fg_mask = (1 << partition_num) - 1;
    uint64 bg_mask = (1 << 20) - 1 - fg_mask;
    m_monitor->m_MSR_handle[0]->write(IA32_L3_MASK + 0, fg_mask);
    m_monitor->m_MSR_handle[0]->write(IA32_L3_MASK + 1, bg_mask);
}
