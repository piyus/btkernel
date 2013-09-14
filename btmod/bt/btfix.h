#ifndef BTFIX_H
#define BTFIX_H

struct btmod;
struct btmod_vcpu;

void push_orig_eips(struct btmod *bt);
uint32_t fetch_eip_and_prev_tb(struct btmod_vcpu *v, 
	uint32_t cur_eip, uint32_t *eip, uint32_t *prev_tb);
bool add_bt_exception_table(uint32_t eip, uint32_t fixup);
const struct exception_table_entry *
__search_exception_tables(uint32_t addr);
int init_exception_table(void __user *table, int len);
bool extable_is_inited(void);

#endif
