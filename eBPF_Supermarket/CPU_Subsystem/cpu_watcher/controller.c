// Copyright 2023 The LMP Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://github.com/linuxkerneltravel/lmp/blob/develop/LICENSE
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// author: albert_xuu@163.com zhangxy1016304@163.com zhangziheng0525@163.com
//
// used to control the execution of proc_image tool
#include <stdio.h>
#include <stdbool.h>
#include <argp.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "cpu_watcher_helper.h"

static struct env {
    // 1代表activate；2代表unactivate；3代表finish
    int usemode;
    bool SAR;
    bool CS_DELAY;
    bool SYSCALL_DELAY;
    bool MIN_US_SET;
    int MIN_US;
    bool PREEMPT;
    bool SCHEDULE_DELAY;
    bool MQ_DELAY;
    int freq;
} env = {
    .usemode = 0,
    .SAR = false,
    .CS_DELAY = false,
    .SYSCALL_DELAY = false,
    .MIN_US_SET = false,
    .MIN_US = 10000,
    .PREEMPT = false,
    .SCHEDULE_DELAY = false,
    .MQ_DELAY = false,
    .freq = 99,
};

const char argp_program_doc[] ="Trace process to get cpu watcher.\n";

static const struct argp_option opts[] = {
    { "activate", 'a', NULL, 0, "Set startup policy of proc_image tool" },
    { "unactivate", 'u', NULL, 0, "Initialize to the original unactivated state" },
    { "finish", 'f', NULL, 0, "Finish to run eBPF tool" },
    {"libbpf_sar", 's', 0, 0, "Print sar_info (the data of cpu)" },
    {"cs_delay", 'c', 0, 0, "Print cs_delay (the data of cpu)" },
    {"syscall_delay", 'S', 0, 0, "Print syscall_delay (the data of syscall)" },
    {"preempt_time", 'p', 0, 0, "Print preempt_time (the data of preempt_schedule)" },
    {"schedule_delay", 'd', 0, 0, "Print schedule_delay (the data of cpu)" },
    {"schedule_delay_min_us_set", 'e', "THRESHOLD", 0, "Print scheduling delays that exceed the threshold (the data of cpu)" },
    {"mq_delay", 'm', 0, 0, "Print mq_delay(the data of proc)" },    
	// { "pid", 'p', "PID", 0, "Process ID to trace" },
    // { "tgid", 'P', "TGID", 0, "Thread group to trace" },
    // { "cpuid", 'c', "CPUID", 0, "Set For Tracing  per-CPU Process(other processes don't need to set this parameter)" },
    // { "time", 't', "TIME-SEC", 0, "Max Running Time(0 for infinite)" },
    // { "myproc", 'm', NULL, 0, "Trace the process of the tool itself (not tracked by default)" },
    // { "resource", 'r', NULL, 0, "Collects resource usage information about processes" },
    // { "keytime", 'k', "KEYTIME", 0, "Collects keytime information about processes(0:except CPU kt_info,1:all kt_info,any 0 or 1 when deactivated)" },
    // { "lock", 'l', NULL, 0, "Collects lock information about processes" },
    // { "syscall", 's', "SYSCALLS", 0, "Collects syscall sequence (1~50) information about processes(any 1~50 when deactivated)" },
    // { "schedule", 'S', NULL, 0, "Collects schedule information about processes (trace tool process)" },
    { NULL, 'h', NULL, OPTION_HIDDEN, "show the full help" },
    {},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
    switch (key) {
        case 'a':
            env.usemode = 1;
            break;
        case 'u':
            env.usemode = 2;
            break;
        case 'f':
            env.usemode = 3;
            break;
        case 's':
            env.SAR = true;
            break;
        case 'c':
            env.CS_DELAY = true;
            break;
        case 'S':
            env.SYSCALL_DELAY = true;
            break;
        case 'p':
            env.PREEMPT = true;
            break;
        case 'd':
            env.SCHEDULE_DELAY = true;
            break;
        case 'e':
            env.MIN_US_SET = true;
            if (arg) {
                env.MIN_US = strtol(arg, NULL, 10);
                if (env.MIN_US <= 0) {
                    fprintf(stderr, "Invalid value for min_us: %d\n", env.MIN_US);
                    argp_usage(state);
                }
            } else {
                env.MIN_US = 10000;
            }
            break;
        case 'm':
            env.MQ_DELAY = true;
            break;			
        case 'h':
				argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
				break;
        default:
				return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

int deactivate_mode(){
    int err;

    if(env.SAR){
        struct sar_ctrl sar_ctrl = {false,0};
        err = update_sar_ctrl_map(sar_ctrl);
        if(err < 0) return err;
    }
    if(env.CS_DELAY){
        struct cs_ctrl cs_ctrl = {false,0};
        err = update_cs_ctrl_map(cs_ctrl);
        if(err < 0) return err;
    }
    if(env.SYSCALL_DELAY){
        struct sc_ctrl sc_ctrl = {false,0};
        err = update_sc_ctrl_map(sc_ctrl);
        if(err < 0) return err;
    }
    if(env.PREEMPT){
        struct preempt_ctrl preempt_ctrl = {false,0};
        err = update_preempt_ctrl_map(preempt_ctrl);
        if(err < 0) return err;
    }
    if(env.SCHEDULE_DELAY){
        struct schedule_ctrl schedule_ctrl = {false,false,10000,0};
        err = update_schedule_ctrl_map(schedule_ctrl);
        if(err < 0) return err;
    }
    if(env.MQ_DELAY){
        struct mq_ctrl mq_ctrl = {false,0};
        err = update_mq_ctrl_map(mq_ctrl);
        if(err < 0) return err;
    }    
    return 0;
}

static void sig_handler(int signo)
{
	deactivate_mode();
}

int main(int argc, char **argv)
{
    int err;
    static const struct argp argp = {
		.options = opts,
		.parser = parse_arg,
		.doc = argp_program_doc,
	};

    err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err)
		return err;

    signal(SIGALRM,sig_handler);
	signal(SIGINT,sig_handler);
	signal(SIGTERM,sig_handler);

    if(env.usemode == 1){                   // activate mode
        if(env.SAR){
            struct sar_ctrl sar_ctrl = {true,SAR_WACTHER};
            err = update_sar_ctrl_map(sar_ctrl);
            if(err < 0) return err;
        }

        if(env.CS_DELAY){
            struct cs_ctrl cs_ctrl = {true,CS_WACTHER};
            err = update_cs_ctrl_map(cs_ctrl);
            if(err < 0) return err;
        }

        if(env.SYSCALL_DELAY){
            struct sc_ctrl sc_ctrl = {true,SC_WACTHER};
            err = update_sc_ctrl_map(sc_ctrl);
            if(err < 0) return err;
        }

        if(env.PREEMPT){
            struct preempt_ctrl preempt_ctrl = {true,PREEMPT_WACTHER};
            err = update_preempt_ctrl_map(preempt_ctrl);
            if(err < 0) return err;
        }

        if(env.SCHEDULE_DELAY){
            /*
             *1.未设置env.MIN_US_SET时, prev_watcher = SCHEDULE_WACTHER + 0;输出方式为schedule输出 
             *2.已设置env.MIN_US_SET时, prev_watcher = SCHEDULE_WACTHER + 1;输出方式为-e输出
             */
            struct schedule_ctrl schedule_ctrl = {true,env.MIN_US_SET,env.MIN_US,SCHEDULE_WACTHER+env.MIN_US_SET};
            err = update_schedule_ctrl_map(schedule_ctrl);
            if(err < 0) return err;
        }

        if(env.MQ_DELAY){
            struct mq_ctrl mq_ctrl = {true,MQ_WACTHER};
            err = update_mq_ctrl_map(mq_ctrl);
            if(err < 0) return err;
        }
    }else if(env.usemode == 2){             // deactivate mode
        err = deactivate_mode();
        if(err<0){
            fprintf(stderr, "Failed to deactivate\n");
            return err;
        }
    }else if(env.usemode == 3){             // finish mode
        const char *command = "pkill cpu_watcher";
        int status = system(command);
        if (status == -1) {
            perror("system");
        }
    }else{
        // 输出help信息
        printf("Please enter the usage mode(activate/deactivate/finish) before selecting the function\n");
        argp_help(&argp, stderr, ARGP_HELP_LONG, argv[0]);
    }

    return 0;
}
