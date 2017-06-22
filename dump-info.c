#include <sys/types.h>
#include <ppsi/ppsi.h>
#include <softpll_ng.h>

#include "dump-info.h"

struct dump_info  dump_info[] = {

/* map for fields of ppsi structures */
#undef DUMP_STRUCT
#define DUMP_STRUCT struct pp_globals

	DUMP_HEADER("pp_globals"),
	DUMP_FIELD(pointer, pp_instances),	/* FIXME: follow this */
	DUMP_FIELD(pointer, servo),		/* FIXME: follow this */
	DUMP_FIELD(pointer, rt_opts),
	DUMP_FIELD(pointer, defaultDS),
	DUMP_FIELD(pointer, currentDS),
	DUMP_FIELD(pointer, parentDS),
	DUMP_FIELD(pointer, timePropertiesDS),
	DUMP_FIELD(int, ebest_idx),
	DUMP_FIELD(int, ebest_updated),
	DUMP_FIELD(int, nlinks),
	DUMP_FIELD(int, max_links),
	//DUMP_FIELD(struct pp_globals_cfg cfg),
	DUMP_FIELD(int, rxdrop),
	DUMP_FIELD(int, txdrop),
	DUMP_FIELD(pointer, arch_data),
	DUMP_FIELD(pointer, global_ext_data),

#undef DUMP_STRUCT
#define DUMP_STRUCT DSDefault /* Horrible typedef */

	DUMP_HEADER("DSDefault"),
	DUMP_FIELD(Boolean, twoStepFlag),
	DUMP_FIELD(ClockIdentity, clockIdentity),
	DUMP_FIELD(UInteger16, numberPorts),
	DUMP_FIELD(ClockQuality, clockQuality),
	DUMP_FIELD(UInteger8, priority1),
	DUMP_FIELD(UInteger8, priority2),
	DUMP_FIELD(UInteger8, domainNumber),
	DUMP_FIELD(Boolean, slaveOnly),

#undef DUMP_STRUCT
#define DUMP_STRUCT DSCurrent /* Horrible typedef */

	DUMP_HEADER("DSCurrent"),
	DUMP_FIELD(UInteger16, stepsRemoved),
	DUMP_FIELD(pp_time, offsetFromMaster),
	DUMP_FIELD(pp_time, meanPathDelay), /* oneWayDelay */
	DUMP_FIELD(UInteger16, primarySlavePortNumber),

#undef DUMP_STRUCT
#define DUMP_STRUCT DSParent /* Horrible typedef */

	DUMP_HEADER("DSParent"),
	DUMP_FIELD(PortIdentity, parentPortIdentity),
	DUMP_FIELD(UInteger16, observedParentOffsetScaledLogVariance),
	DUMP_FIELD(Integer32, observedParentClockPhaseChangeRate),
	DUMP_FIELD(ClockIdentity, grandmasterIdentity),
	DUMP_FIELD(ClockQuality, grandmasterClockQuality),
	DUMP_FIELD(UInteger8, grandmasterPriority1),
	DUMP_FIELD(UInteger8, grandmasterPriority2),

#undef DUMP_STRUCT
#define DUMP_STRUCT DSTimeProperties /* Horrible typedef */

	DUMP_HEADER("DSTimeProperties"),
	DUMP_FIELD(Integer16, currentUtcOffset),
	DUMP_FIELD(Boolean, currentUtcOffsetValid),
	DUMP_FIELD(Boolean, leap59),
	DUMP_FIELD(Boolean, leap61),
	DUMP_FIELD(Boolean, timeTraceable),
	DUMP_FIELD(Boolean, frequencyTraceable),
	DUMP_FIELD(Boolean, ptpTimescale),
	DUMP_FIELD(Enumeration8, timeSource),

#undef DUMP_STRUCT
#define DUMP_STRUCT struct wr_servo_state

	DUMP_HEADER("servo_state"),
	DUMP_FIELD_SIZE(char, if_name, 16),
	DUMP_FIELD(unsigned_long, flags),
	DUMP_FIELD(int, state),
	DUMP_FIELD(Integer32, delta_tx_m),
	DUMP_FIELD(Integer32, delta_rx_m),
	DUMP_FIELD(Integer32, delta_tx_s),
	DUMP_FIELD(Integer32, delta_rx_s),
	DUMP_FIELD(Integer32, fiber_fix_alpha),
	DUMP_FIELD(Integer32, clock_period_ps),
	DUMP_FIELD(pp_time, mu),		/* half of the RTT */
	DUMP_FIELD(Integer64, picos_mu),
	DUMP_FIELD(Integer32, cur_setpoint),
	DUMP_FIELD(Integer64, delta_ms),
	DUMP_FIELD(UInteger32, update_count),
	DUMP_FIELD(int, tracking_enabled),
	DUMP_FIELD_SIZE(char, servo_state_name, 32),
	DUMP_FIELD(Integer64, skew),
	DUMP_FIELD(Integer64, offset),
	DUMP_FIELD(UInteger32, n_err_state),
	DUMP_FIELD(UInteger32, n_err_offset),
	DUMP_FIELD(UInteger32, n_err_delta_rtt),
	DUMP_FIELD(pp_time, update_time),
	DUMP_FIELD(pp_time, t1),
	DUMP_FIELD(pp_time, t2),
	DUMP_FIELD(pp_time, t3),
	DUMP_FIELD(pp_time, t4),
	DUMP_FIELD(pp_time, t5),
	DUMP_FIELD(pp_time, t6),
	DUMP_FIELD(Integer64, delta_ms_prev),
	DUMP_FIELD(int, missed_iters),

#undef DUMP_STRUCT
#define DUMP_STRUCT struct pp_instance

	DUMP_HEADER("pp_instance"),
	DUMP_FIELD(int, state),
	DUMP_FIELD(int, next_state),
	DUMP_FIELD(int, next_delay),
	DUMP_FIELD(int, is_new_state),
	DUMP_FIELD(pointer, current_state_item),
	DUMP_FIELD(pointer, arch_data),
	DUMP_FIELD(pointer, ext_data),
	DUMP_FIELD(unsigned_long, d_flags),
	DUMP_FIELD(unsigned_char, flags),
	DUMP_FIELD(int, role),
	DUMP_FIELD(int, proto),
	DUMP_FIELD(int, mech),
	DUMP_FIELD(pointer, glbs),
	DUMP_FIELD(pointer, n_ops),
	DUMP_FIELD(pointer, t_ops),
	DUMP_FIELD(pointer, __tx_buffer),
	DUMP_FIELD(pointer, __rx_buffer),
	DUMP_FIELD(pointer, tx_frame),
	DUMP_FIELD(pointer, rx_frame),
	DUMP_FIELD(pointer, tx_ptp),
	DUMP_FIELD(pointer, rx_ptp),

	/* This is a sub-structure */
	DUMP_FIELD(int, ch[0].fd),
	DUMP_FIELD(pointer, ch[0].custom),
	DUMP_FIELD(pointer, ch[0].arch_data),
	DUMP_FIELD_SIZE(bina, ch[0].addr, 6),
	DUMP_FIELD(int, ch[0].pkt_present),
	DUMP_FIELD(int, ch[1].fd),
	DUMP_FIELD(pointer, ch[1].custom),
	DUMP_FIELD(pointer, ch[1].arch_data),
	DUMP_FIELD_SIZE(bina, ch[1].addr, 6),
	DUMP_FIELD(int, ch[1].pkt_present),

	DUMP_FIELD(ip_address, mcast_addr[0]),
	DUMP_FIELD(ip_address, mcast_addr[1]),
	DUMP_FIELD(int, tx_offset),
	DUMP_FIELD(int, rx_offset),
	DUMP_FIELD_SIZE(bina, peer, 6),
	DUMP_FIELD(uint16_t, peer_vid),

	DUMP_FIELD(pp_time, t1),
	DUMP_FIELD(pp_time, t2),
	DUMP_FIELD(pp_time, t3),
	DUMP_FIELD(pp_time, t4),
	DUMP_FIELD(pp_time, t5),
	DUMP_FIELD(pp_time, t6),
	DUMP_FIELD(UInteger64, syncCF),
	DUMP_FIELD(pp_time, last_rcv_time),
	DUMP_FIELD(pp_time, last_snt_time),
	DUMP_FIELD(UInteger16, frgn_rec_num),
	DUMP_FIELD(Integer16,  frgn_rec_best),
	//DUMP_FIELD(struct pp_frgn_master frgn_master[PP_NR_FOREIGN_RECORDS]),
	DUMP_FIELD(pointer, portDS),
	//DUMP_FIELD(unsigned long timeouts[__PP_TO_ARRAY_SIZE]),
	DUMP_FIELD(UInteger16, recv_sync_sequence_id),
	//DUMP_FIELD(UInteger16 sent_seq[__PP_NR_MESSAGES_TYPES]),
	DUMP_FIELD_SIZE(bina, received_ptp_header, sizeof(MsgHeader)),
	//DUMP_FIELD(pointer, iface_name),
	//DUMP_FIELD(pointer, port_name),
	DUMP_FIELD(int, port_idx),
	DUMP_FIELD(int, vlans_array_len),
	/* FIXME: array */
	DUMP_FIELD(int, nvlans),

	/* sub structure */
	DUMP_FIELD_SIZE(char, cfg.port_name, 16),
	DUMP_FIELD_SIZE(char, cfg.iface_name, 16),
	DUMP_FIELD(int, cfg.ext),
	DUMP_FIELD(int, cfg.ext),
	DUMP_FIELD(int, cfg.mech),

	DUMP_FIELD(unsigned_long, ptp_tx_count),
	DUMP_FIELD(unsigned_long, ptp_rx_count),

#undef DUMP_STRUCT
#define DUMP_STRUCT struct softpll_state

	DUMP_HEADER("softpll"),
	DUMP_FIELD(int, mode),
	DUMP_FIELD(int, seq_state),
	DUMP_FIELD(int, dac_timeout),
	DUMP_FIELD(int, delock_count),
	DUMP_FIELD(uint32_t, irq_count),
	DUMP_FIELD(int, mpll_shift_ps),
	DUMP_FIELD(int, helper.p_adder),
	DUMP_FIELD(int, helper.p_setpoint),
	DUMP_FIELD(int, helper.tag_d0),
	DUMP_FIELD(int, helper.ref_src),
	DUMP_FIELD(int, helper.sample_n),
	/* FIXME: missing helper.pi etc.. */
	DUMP_FIELD(int, ext.enabled),
	DUMP_FIELD(int, ext.align_state),
	DUMP_FIELD(int, ext.align_timer),
	DUMP_FIELD(int, ext.align_target),
	DUMP_FIELD(int, ext.align_step),
	DUMP_FIELD(int, ext.align_shift),
	DUMP_FIELD(int, mpll.state),
	/* FIXME: mpll.pi etc */
	DUMP_FIELD(int, mpll.adder_ref),
	DUMP_FIELD(int, mpll.adder_out),
	DUMP_FIELD(int, mpll.tag_ref),
	DUMP_FIELD(int, mpll.tag_out),
	DUMP_FIELD(int, mpll.tag_ref_d),
	DUMP_FIELD(int, mpll.tag_out_d),
	DUMP_FIELD(int, mpll.phase_shift_target),
	DUMP_FIELD(int, mpll.phase_shift_current),
	DUMP_FIELD(int, mpll.id_ref),
	DUMP_FIELD(int, mpll.id_out),
	DUMP_FIELD(int, mpll.sample_n),
	DUMP_FIELD(int, mpll.dac_index),
	DUMP_FIELD(int, mpll.enabled),

#undef DUMP_STRUCT
#define DUMP_STRUCT struct spll_fifo_log

	DUMP_HEADER("pll_fifo"),
	DUMP_FIELD(uint32_t, trr),
	DUMP_FIELD(uint32_t, tstamp),
	DUMP_FIELD(uint32_t, duration),
	DUMP_FIELD(uint16_t, irq_count),
	DUMP_FIELD(uint16_t, tag_count),
	/* FIXME: aux_state and ptracker_state -- variable-len arrays */

#undef DUMP_STRUCT
#define DUMP_STRUCT struct spll_stats

	DUMP_HEADER("stats"),
	DUMP_FIELD(uint32_t, magic),
	DUMP_FIELD(int, ver),
	DUMP_FIELD(int, sequence),
	DUMP_FIELD(int, mode),
	DUMP_FIELD(int, irq_cnt),
	DUMP_FIELD(int, seq_state),
	DUMP_FIELD(int, align_state),
	DUMP_FIELD(int, H_lock),
	DUMP_FIELD(int, M_lock),
	DUMP_FIELD(int, H_y),
	DUMP_FIELD(int, M_y),
	DUMP_FIELD(int, del_cnt),
	DUMP_FIELD(int, start_cnt),
	DUMP_FIELD_SIZE(char, commit_id, 32),
	DUMP_FIELD_SIZE(char, build_date, 16),
	DUMP_FIELD_SIZE(char, build_time, 16),
	DUMP_FIELD_SIZE(char, build_by, 32),

	DUMP_HEADER("end"),

};
