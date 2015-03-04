#include "procsim.hpp"

proc_settings_t cpu;

std::vector<proc_inst_ptr_t> all_instrs;

std::deque<proc_inst_ptr_t> dispatching_queue;
std::vector<proc_inst_ptr_t> scheduling_queue;
int scheduling_queue_limit;

std::unordered_map<uint32_t, register_info_t> register_file;

std::vector<proc_cdb_t> cdb;
std::unordered_map<uint32_t, uint32_t> fu_cnt;


/**
 * Subroutine for initializing the processor. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @r Number of r result buses
 * @k0 Number of k0 FUs
 * @k1 Number of k1 FUs
 * @k2 Number of k2 FUs
 * @f Number of instructions to fetch
 */
void setup_proc(proc_stats_t *p_stats, uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f, uint64_t begin_dump, uint64_t end_dump) {
    p_stats->retired_instruction = 0;
    p_stats->cycle_count = 1;

    cpu = proc_settings_t(f, begin_dump, end_dump);

    for(int i = 0; i < 64; i++){
        register_file[i] = {true};    
    }

    scheduling_queue_limit = 2 * (k0 + k1 + k2);
	scheduling_queue.resize(scheduling_queue_limit);
    cdb.resize(r, {true});
    fu_cnt[0] = k0;
    fu_cnt[1] = k1;
    fu_cnt[2] = k2;
}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC, average fire rate etc.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) {
    p_stats->avg_disp_size = p_stats->sum_disp_size / p_stats->cycle_count;
    p_stats->avg_inst_retired = p_stats->retired_instruction * 1.f / p_stats->cycle_count; 
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void run_proc(proc_stats_t* p_stats) {   
    while (!cpu.finished) {
        // invoke pipline for current cycle
        state_update(p_stats, cycle_half_t::FIRST);
        execute(p_stats, cycle_half_t::FIRST);
        schedule(p_stats, cycle_half_t::FIRST);
        dispatch(p_stats, cycle_half_t::FIRST);

        state_update(p_stats, cycle_half_t::SECOND);

        if (!cpu.finished){
            execute(p_stats, cycle_half_t::SECOND);
            schedule(p_stats, cycle_half_t::SECOND);
            dispatch(p_stats, cycle_half_t::SECOND);
            instr_fetch_and_decode(p_stats, cycle_half_t::SECOND);            
        
            p_stats->cycle_count++;
        }
    }
    
    // print result
    if(cpu.begin_dump > 0){
        std::cout << "INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE" << std::endl;

        for (auto &i : all_instrs){
            if(i->id >= cpu.begin_dump && i->id <= cpu.end_dump){
                std::cout << i->id << "\t"
                          << i->cycle_fetch_decode << "\t" 
                          << i->cycle_dispatch << "\t"
                          << i->cycle_schedule << "\t"
                          << i->cycle_execute << "\t"
                          << i->cycle_status_update << std::endl;  
            }
        }
        std::cout << std::endl;
    }
}

/** STATE UPDATE stage */
void state_update(proc_stats_t* p_stats, const cycle_half_t &half) {
    if (half == cycle_half_t::FIRST) {
        // record instr entry cycle
        for (auto instr : scheduling_queue) {
            if (instr != nullptr && instr->executed && !instr->cycle_status_update) {
                instr->cycle_status_update = p_stats->cycle_count;              
 			}
        }        
    } else {
        // delete instructions from scheduling queue
        auto it = scheduling_queue.begin();
        while(it != scheduling_queue.end()){
            auto instr = *it;

			if (!instr) {
				it++;
				continue;
			}

            if(instr->cycle_status_update){
                it = scheduling_queue.erase(it);
                p_stats->retired_instruction++;
            }else{
                it++;
            }
        }
        
        if (cpu.read_finished && p_stats->retired_instruction == cpu.read_cnt) 
            cpu.finished = true;        
    }
}

/** EXECUTE stage */

// find free cdb to update the tag 
// 0 - No free cdb
// 1 - Free cdb found and updated
int find_free_cdb(proc_inst_ptr_t instr){
        for (auto c : cdb) {
			if (c.free == true) {
				c.free = false;
				c.tag = instr->id;
				return 1;
			}
		}
		return 0;
}

void execute(proc_stats_t* p_stats, const cycle_half_t &half) {
    if (half == cycle_half_t::FIRST) {
        // record instr entry cycle
        for (auto instr : scheduling_queue) {
            if (instr != nullptr && instr->fired == true && !instr->cycle_execute) {
				// update the CDB with the tag
				if (instr->dest_reg != -1) {
					if (!find_free_cdb(instr)) {
						continue;
					}
				}
				instr->cycle_execute = p_stats->cycle_count;
				instr->executed = true;
            }
        }
    } else {
    }
}

/** SCHEDULE stage - data dependency and free functional unit */ 

// check data dependency from the register file 
void check_data_dependency(proc_inst_ptr_t instr) {
	if (instr->src_reg[0] != -1) {
		if (register_file[instr->src_reg[0]].ready == false) {
			instr->src_tag[0] = register_file[instr->src_reg[0]].tag;
			instr->src_ready[0] = false;
		} else {
			instr->src_ready[0] = true;
			instr->src_tag[0] = 0;
		}
	}

	if (instr->src_reg[1] != -1) {
		if (register_file[instr->src_reg[1]].ready == false) {
			instr->src_tag[1] = register_file[instr->src_reg[1]].tag;
			instr->src_ready[1] = false;
		} else {
			instr->src_ready[1] = true;
			instr->src_tag[1] = 0;
		}
	}
}

void update_register_from_cdb(){
	for (auto c : cdb) {
		if (c.free == false) {
			for(int i = 0; i < 64; i++){
				if (register_file[i].ready == false) {
					if (register_file[i].tag == c.tag) {
						register_file[i].ready = true;
						register_file[i].tag = 0;
					}
				}
			}
		}
	}
}

void update_data_from_cdb(proc_inst_ptr_t instr) {
	for (auto c : cdb) {
		if (c.free == false) {
			if (instr->src_ready[0] == false) {
				if (instr->src_tag[0] == c.tag) {
					instr->src_tag[0] = 0;
					instr->src_ready[0] = true;
				}
			}
			if (instr->src_ready[1] == false) {
				if (instr->src_tag[1] == c.tag) {
					instr->src_tag[1] = 0;
					instr->src_ready[1] = true;
				}
			}
		}
	}
}

int instr_src_available(proc_inst_ptr_t instr) {
	if ((instr->src_ready[0] == true) && (instr->src_ready[1] == true))	{
		return 1;
	}
	return 0;
}

void schedule(proc_stats_t* p_stats, const cycle_half_t &half) {
    if (half == cycle_half_t::FIRST) {
		// update the register file using CDB
        // record instr entry cycle
		update_register_from_cdb();
        for (auto instr : scheduling_queue) {
            if (instr == nullptr || instr->fire)
                continue;

			check_data_dependency(instr);

            if (!instr->cycle_schedule) {
                instr->cycle_schedule = p_stats->cycle_count;
            } 
            
            instr->fire = true;
        }
    } else {
        // fire all marked instructions if possible
        for (auto instr : scheduling_queue) {
            if (instr == nullptr)
                continue;

            if (instr->fire && !instr->fired) {
				update_data_from_cdb(instr);
				if (!instr_src_available(instr)) {
					continue;
				}
                instr->fired = true;
				// when the instruction is fired, mark the register
				// that is using it as busy.
				if (instr->dest_reg != -1) {
					register_file[instr->dest_reg].ready = false;
					register_file[instr->dest_reg].tag = instr->id;
				}
            }
        }
    }
}

/** DISPATCH stage */

// Get the size of the schedule_queue subtracting the instructions that will be
// retired in the second half of the cycle. return: size of sched queue.

int get_sched_size(void) {
	int size = 0;

    for (auto instr : scheduling_queue) {
		if (!instr) {
			continue;
		}
        if((instr->reserved) && (!instr->cycle_status_update)){
			size++;
        }
    }
	return size;
}

void dispatch(proc_stats_t* p_stats, const cycle_half_t &half) {
    if (half == cycle_half_t::FIRST) {    
		int sched_size = get_sched_size();

        if (p_stats->max_disp_size < dispatching_queue.size())
            p_stats->max_disp_size = dispatching_queue.size();
            
        p_stats->sum_disp_size += dispatching_queue.size();

        for (auto instr : dispatching_queue) {
			if (sched_size < scheduling_queue_limit) {
              instr->reserved = true;
			  ++sched_size;
			}
        }
    } else {
        while (!dispatching_queue.empty()) {
            auto instr = dispatching_queue.front();
            
            if (!instr->reserved)
                break;
                
            scheduling_queue.push_back(instr);

            dispatching_queue.pop_front();
        }
    }
}

/** INSTR-FETCH & DECODE stage */
void instr_fetch_and_decode(proc_stats_t* p_stats, const cycle_half_t &half) {
    if (half == cycle_half_t::SECOND) {          
        // read the next instructions 
        if (!cpu.read_finished){
            for (uint64_t i = 0; i < cpu.f; i++) { 
                proc_inst_ptr_t instr = proc_inst_ptr_t(new proc_inst_t());
              
                all_instrs.push_back(instr);
                                
                if (read_instruction(instr.get())) { 
                    // reset counters
                    instr->id = cpu.read_cnt + 1;

                    instr->fire = false;
                    instr->fired = false;
                    instr->executed = false;

					instr->src_ready[0] = true;
					instr->src_ready[1] = true;

                    instr->cycle_fetch_decode = p_stats->cycle_count;
                    instr->cycle_dispatch = p_stats->cycle_count + 1;
                    instr->cycle_schedule = 0;
                    instr->cycle_execute = 0;
                    instr->cycle_status_update = 0;                               
                    
                    dispatching_queue.push_back(instr);                                              
                    cpu.read_cnt++;                     
                } else {
                    all_instrs.pop_back();
                
                    cpu.read_finished = true;  
                    break;
                }
            }
        }   
    }
}





