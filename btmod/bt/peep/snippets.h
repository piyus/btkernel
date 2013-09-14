#ifndef PEEP_SNIPPETS_H
#define PEEP_SNIPPETS_H

extern peepgen_label_t peep_snippet_exit_tb;
extern peepgen_label_t peep_snippet_emit_edge1;
extern peepgen_label_t peep_snippet_save_reg;
extern peepgen_label_t peep_snippet_load_reg;
extern peepgen_label_t peep_snippet_ret;
extern peepgen_label_t peep_snippet_mov_mem_to_reg;
extern peepgen_label_t peep_snippet_movb_regaddr_to_al;
extern peepgen_label_t peep_snippet_movb_al_to_regaddr;
extern peepgen_label_t peep_snippet_movw_regaddr_to_ax;
extern peepgen_label_t peep_snippet_movw_ax_to_regaddr;
extern peepgen_label_t peep_snippet_movl_regaddr_to_eax;
extern peepgen_label_t peep_snippet_movl_eax_to_regaddr;
extern peepgen_label_t peep_snippet_jump_to_monitor;
extern peepgen_label_t peep_snippet_inc_sreg0;
extern peepgen_label_t peep_snippet_inc_sreg0f;
extern peepgen_label_t peep_snippet_idt_vector;
extern peepgen_label_t peep_snippet_pushfl;
extern peepgen_label_t peep_snippet_popfl;
extern peepgen_label_t peep_snippet_push_treg;
extern peepgen_label_t peep_snippet_pop_treg;
extern peepgen_label_t peep_snippet_count_loads;
extern peepgen_label_t peep_snippet_remap_1_b_r; //%vr1d: no_esp, %tr0d: no_esp 
extern peepgen_label_t peep_snippet_remap_1_w_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_1_l_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_1_b_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_1_w_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_1_l_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_2_b_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_2_w_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_2_l_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_2_b_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_2_w_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_2_l_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_4_b_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_4_w_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_4_l_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_4_b_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_4_w_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_4_l_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_8_b_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_8_w_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_8_l_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_8_b_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_8_w_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_8_l_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_1_b_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_1_w_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_1_l_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_1_b_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_1_w_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_1_l_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_2_b_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_2_w_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_2_l_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_2_b_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_2_w_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_2_l_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_4_b_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_4_w_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_4_l_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_4_b_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_4_w_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_4_l_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_8_b_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_8_w_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_8_l_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_8_b_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_8_w_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_8_l_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rbase_b_r; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rbase_w_r; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rbase_l_r; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rbase_b_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rbase_w_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rbase_l_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rdisp_b_r; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rdisp_w_r; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rdisp_l_r; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rdisp_b_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rdisp_w_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rdisp_l_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_movs_b; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_movs_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_movs_l; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_stos_b; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_stos_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_stos_l; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_scas_b; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_scas_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_scas_l; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_cmps_b; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_cmps_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_cmps_l; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_1_b_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_1_w_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_1_l_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_1_b_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_1_w_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_1_l_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_2_b_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_2_w_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_2_l_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_2_b_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_2_w_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_2_l_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_4_b_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_4_w_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_4_l_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_4_b_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_4_w_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_4_l_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_8_b_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_8_w_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_8_l_r; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_8_b_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_8_w_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_remap_safe_8_l_w; //%vr1d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_1_b_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_1_w_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_1_l_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_1_b_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_1_w_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_1_l_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_2_b_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_2_w_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_2_l_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_2_b_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_2_w_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_2_l_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_4_b_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_4_w_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_4_l_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_4_b_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_4_w_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_4_l_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_8_b_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_8_w_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_8_l_r; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_8_b_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_8_w_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rindx_safe_8_l_w; //%vr0d: no_esp, %tr0d: no_esp
extern peepgen_label_t peep_snippet_rbase_safe_b_r; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rbase_safe_w_r; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rbase_safe_l_r; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rbase_safe_b_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rbase_safe_w_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rbase_safe_l_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rdisp_safe_b_r; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rdisp_safe_w_r; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rdisp_safe_l_r; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rdisp_safe_b_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rdisp_safe_w_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_rdisp_safe_l_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_movss_b; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_movss_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_movss_l; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_stoss_b; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_stoss_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_stoss_l; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_scass_b; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_scass_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_scass_l; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_cmpss_b; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_cmpss_w; //%tr0d: no_esp
extern peepgen_label_t peep_snippet_cmpss_l; //%tr0d: no_esp

#endif
