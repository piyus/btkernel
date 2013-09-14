#ifndef INSTRUMENT_H
#define INSTRUMENT_H

struct btmod_vcpu;

void
count_insns(struct btmod_vcpu *v, uint32_t eip_virt,
	uint8_t **optr, uint8_t **eptr, int num_insns);

bool
instrument_memory(struct btmod_vcpu *v, uint32_t eip, 
		insn_t const *insn, uint8_t **optr, 
		uint8_t **eptr, bool tmode);

void peepgen_code(btmod_vcpu *v, int label,
   long *peep_param_buf, uint8_t **gen_code_buf, 
   uint8_t **gen_edge_buf, unsigned long *tx_target, 
   long fallthrough_addr, bool *is_terminating);

int
count_functions(struct btmod_vcpu *v, uint32_t eip,
	 insn_t const *insn, uint8_t **optr, uint8_t **eptr);

void
print_function_table(void);

int
check_load_eip(btmod_vcpu *v, uint32_t eip,
		    insn_t const *insn);

uint32_t
find_a_random_eip(void);

void
instrument_a_random_load(btmod_vcpu *v);

#endif
