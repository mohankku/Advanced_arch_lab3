#include <stdio.h>
#include <cinttypes>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <inttypes.h>
#include "procsim.hpp"

FILE* inFile;

void print_help_and_exit(void) {
    printf("procsim [OPTIONS]\n");
    printf("  -j k0\t\tNumber of k0 FUs\n");
    printf("  -k k1\t\tNumber of k1 FUs\n");
    printf("  -l k2\t\tNumber of k2 FUs\n");   
    printf("  -f N\t\tNumber of instructions to fetch\n");
    printf("  -r R\t\tNumber of result buses\n");
    printf("  -i traces/file.trace\n");
    printf("  -h\t\tThis helpful output\n");
    exit(0);
}

//
// read_instruction
//
//  returns true if an instruction was read successfully
//
bool read_instruction(proc_inst_t* p_inst){
    if(inFile == NULL){
        return false;
    }

    Trace_Rec tr_entry;    

    if (p_inst == NULL){
        fprintf(stderr, "Fetch requires a valid pointer to populate\n");
        return false;
    }

    uint8_t bytes_read = 0;
    bytes_read = fread(&tr_entry, 1, sizeof(Trace_Rec), inFile);
    
    // check for end of trace
    if( bytes_read < sizeof(Trace_Rec)) {
        return false;
    }

    p_inst->instruction_address = tr_entry.inst_addr;

    if(tr_entry.op_type == OP_ALU){
        p_inst->op_code = 0;
    }else if(tr_entry.op_type == OP_LD || tr_entry.op_type == OP_ST){
        p_inst->op_code = 1;
    }else if(tr_entry.op_type == OP_CBR){
        p_inst->op_code = 2;
    }else if(tr_entry.op_type == OP_OTHER){
        p_inst->op_code = 0;
    }

    if(tr_entry.dest_needed == 1){
        p_inst->dest_reg = tr_entry.dest;
    }else{
        p_inst->dest_reg = (-1);
    }

    if(tr_entry.src1_needed == 1){
        p_inst->src_reg[0] = tr_entry.src1_reg;
    }else{
        p_inst->src_reg[0] = (-1);
    }        

    if(tr_entry.src2_needed == 1){
        p_inst->src_reg[1] = tr_entry.src2_reg;
    }else{
        p_inst->src_reg[1] = (-1);
    }    

    return true;
}

void print_statistics(proc_stats_t* p_stats);

int main(int argc, char* argv[]) {
    int opt;
    uint64_t f = DEFAULT_F;
    uint64_t k0 = DEFAULT_K0;
    uint64_t k1 = DEFAULT_K1;
    uint64_t k2 = DEFAULT_K2;
    uint64_t r = DEFAULT_R;

    uint64_t begin_dump; 
    uint64_t end_dump; 

    inFile = NULL;

    /* Read arguments */ 
    char tr_filename[256];    
    char cmd_string[256];    
    while(-1 != (opt = getopt(argc, argv, "r:f:j:k:l:b:e:i:h"))) {
        switch(opt) {
        case 'r':
            r = atoi(optarg);
            break;
        case 'f':
            f = atoi(optarg);
            break;            
        case 'j':
            k0 = atoi(optarg);
            break;
        case 'k':
            k1 = atoi(optarg);
            break;
        case 'l':
            k2 = atoi(optarg);
            break;
        case 'b':
            begin_dump = atoi(optarg);
            break;
        case 'e':
            end_dump = atoi(optarg);
            break;    
        case 'i':
            strcpy(tr_filename, optarg);
            break;
        case 'h':
            /* Fall through */
        default:
            print_help_and_exit();
            break;
        }
    }

    sprintf(cmd_string,"gunzip -c %s", tr_filename);    
    if ((inFile = popen(cmd_string, "r")) == NULL){
        printf("Command string is %s\n", cmd_string);
        printf("Unable to open the trace file with gzip option \n");
    } else {
        printf("Opened file with command: %s \n", cmd_string);
    } 

    printf("Processor Settings\n");
    printf("R: %" PRIu64 "\n", r);
    printf("k0: %" PRIu64 "\n", k0);
    printf("k1: %" PRIu64 "\n", k1);
    printf("k2: %" PRIu64 "\n", k2);
    printf("F: %"  PRIu64 "\n", f);
    printf("\n");

    /* Setup statistics */
    proc_stats_t stats;
    memset(&stats, 0, sizeof(proc_stats_t));    

    /* Setup the processor */
    setup_proc(&stats, r, k0, k1, k2, f, begin_dump, end_dump);

    /* Run the processor */
    run_proc(&stats);

    /* Finalize stats */
    complete_proc(&stats);

    print_statistics(&stats);

    return 0;
}

void print_statistics(proc_stats_t* p_stats) {
    printf("Processor stats:\n");
    printf("Total instructions: %lu\n", p_stats->retired_instruction);    
    printf("Total run time (cycles): %lu\n", p_stats->cycle_count);
    printf("Avg inst retired per cycle: %f\n", p_stats->avg_inst_retired);
    printf("Maximum Dispatch queue size: %lu\n", p_stats->max_disp_size);
    printf("Avg Dispatch queue size: %f\n", p_stats->avg_disp_size);    
}

