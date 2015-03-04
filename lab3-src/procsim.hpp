#ifndef PROCSIM_H
#define PROCSIM_H

#define DEFAULT_K0 1
#define DEFAULT_K1 2
#define DEFAULT_K2 3
#define DEFAULT_R 8
#define DEFAULT_F 4

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <vector>
#include <deque>
#include <memory>
#include <utility>
#include <unordered_map>
#include <unordered_set>

typedef enum Op_Type_Enum{
    OP_ALU,             // ALU(ADD/ SUB/ MUL/ DIV) operaiton
    OP_LD,              // load operation
    OP_ST,              // store operation
    OP_CBR,             // Conditional Branch
    OP_OTHER,           // Other Ops
    NUM_OP_TYPE
} Op_Type;

enum cycle_half_t { FIRST, SECOND };

/* Data structure for Trace Record */ 
typedef struct Trace_Rec_Struct {
    uint64_t inst_addr;  // instruction address 
    uint8_t  op_type;    // optype
    uint8_t  dest;       // Destination
    uint8_t  dest_needed; // 
    uint8_t  src1_reg;       // Source Register 1
    uint8_t  src2_reg;       // Source Register 2
    uint8_t  src1_needed; // Source Register 1 needed by this instruction
    uint8_t  src2_needed; // Source Register 2 needed to this instruction
    uint8_t  cc_read;    // Conditional Code Read
    uint8_t  cc_write;   // Conditional Code Write
    uint64_t mem_addr;   // Load / Store Memory Address
    uint8_t  mem_write;  // Write 
    uint8_t  mem_read;   // Read
    uint8_t  br_dir;     // Branch Direction Taken / Not Taken
    uint64_t br_target;  // Target Address of Branch
} Trace_Rec;

// our extended instruction structure
typedef struct _proc_inst_t
{
    uint32_t instruction_address;
    int32_t op_code;
    int32_t dest_reg;
    int32_t src_reg[2];
    
    uint32_t id;
    uint64_t dest_tag;
    uint64_t src_tag[2];
    bool src_ready[2];
    
    bool reserved;
    bool fire;
    bool fired;
    bool executed;
    
    uint64_t cycle_fetch_decode;
    uint64_t cycle_dispatch;
    uint64_t cycle_schedule;
    uint64_t cycle_execute;
    uint64_t cycle_status_update;
} proc_inst_t;

typedef std::shared_ptr<proc_inst_t> proc_inst_ptr_t;

typedef struct _proc_stats_t
{
    unsigned long retired_instruction;
    unsigned long cycle_count;
    float avg_inst_retired;
    unsigned long max_disp_size;
    double sum_disp_size;
    float avg_disp_size;
} proc_stats_t;

// a cdb representation
struct proc_cdb_t {
    bool free;
    uint32_t reg;
    uint32_t tag;
};

// our global state structure for the processor
struct proc_settings_t {
    proc_settings_t() { }
    proc_settings_t(uint64_t f, uint64_t begin_dump, uint64_t end_dump) 
        : f(f), begin_dump(begin_dump), end_dump(end_dump),
        read_cnt(0), read_finished(false), finished(false) { }

    uint64_t f;

    uint64_t begin_dump;
    uint64_t end_dump;
    
    uint64_t read_cnt;
    bool read_finished;
    bool finished;
};

struct register_info_t {
    bool ready;
    uint64_t tag;
};

struct rs_status_t {
	bool busy;
	int32_t opcode;
	int32_t src_reg[2];
	uint64_t src_tag[2];
};

bool read_instruction(proc_inst_t* p_inst);

void setup_proc(proc_stats_t *p_stats, uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f, uint64_t begin_dump, uint64_t end_dump);
void complete_proc(proc_stats_t* p_stats);
void run_proc(proc_stats_t* p_stats);

// our pipeline stages
void state_update(proc_stats_t* p_stats, const cycle_half_t &half);
void execute(proc_stats_t* p_stats, const cycle_half_t &half);
void schedule(proc_stats_t* p_stats, const cycle_half_t &half);
void dispatch(proc_stats_t* p_stats, const cycle_half_t &half);
void instr_fetch_and_decode(proc_stats_t* p_stats, const cycle_half_t &half);

#endif /* PROCSIM_H */
