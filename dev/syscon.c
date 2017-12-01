/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "syscon.h"
#include <errno.h>
#include <string.h>

struct s_i2c_if i2c_if[2] = {
	{SYSC_GPSR_FMC_SCL, SYSC_GPSR_FMC_SDA, FMC_I2C_DELAY},
	{SYSC_GPSR_SFP_SCL, SYSC_GPSR_SFP_SDA, SFP_I2C_DELAY}
};

volatile struct SYSCON_WB *syscon;

/****************************
 *       BOARD NAME
 ***************************/
void get_hw_name(char *str)
{
	uint32_t val;

	val = syscon->HWIR;
	memcpy(str, &val, HW_NAME_LENGTH-1);
}

/****************************
 *       Flash info
 ***************************/
void get_storage_info(int *memtype, uint32_t *sdbfs_baddr, uint32_t *blocksize)
{
	/* convert sector size from KB to bytes */
	*blocksize = SYSC_HWFR_STORAGE_SEC_R(syscon->HWFR) * 1024;
	*sdbfs_baddr = syscon->SDBFS;
	*memtype = SYSC_HWFR_STORAGE_TYPE_R(syscon->HWFR);
}

/****************************
 *        TIMER
 ***************************/
void timer_init(uint32_t enable)
{
	syscon = (volatile struct SYSCON_WB *)BASE_SYSCON;

	if (enable)
		syscon->TCR |= SYSC_TCR_ENABLE;
	else
		syscon->TCR &= ~SYSC_TCR_ENABLE;
}

uint32_t timer_get_tics(void)
{
	return syscon->TVR;
}

void timer_delay(uint32_t tics)
{
	uint32_t t_end;

	/*
	timer_init(1);
	*/

	t_end = timer_get_tics() + tics;
	while (time_before(timer_get_tics(), t_end))
	       ;
}

static int diag_rw_words, diag_ro_words;

/****************************
 *        AUX Diagnostics
 ***************************/
void diag_read_info(uint32_t *id, uint32_t *ver, uint32_t *nrw, uint32_t *nro)
{
	diag_rw_words = SYSC_DIAG_NW_RW_R(syscon->DIAG_NW);
	diag_ro_words = SYSC_DIAG_NW_RO_R(syscon->DIAG_NW);

	if (id)
		*id = SYSC_DIAG_INFO_ID_R(syscon->DIAG_INFO);
	if (ver)
		*ver = SYSC_DIAG_INFO_VER_R(syscon->DIAG_INFO);
	if (nrw)
		*nrw = diag_rw_words;
	if (nro)
		*nro = diag_ro_words;
}

int diag_read_word(uint32_t adr, int bank, uint32_t *val)
{
	if (!val)
		return -EINVAL;

	if (diag_rw_words == 0) {
		pp_printf("fetching diag_rw_words\n");
		diag_rw_words = SYSC_DIAG_NW_RW_R(syscon->DIAG_NW);
	}
	if (diag_ro_words == 0) {
		pp_printf("fetching diag_ro_words\n");
		diag_ro_words = SYSC_DIAG_NW_RO_R(syscon->DIAG_NW);
	}

	if ((bank == DIAG_RW_BANK && adr >= diag_rw_words) ||
		(bank == DIAG_RO_BANK && adr >= diag_ro_words)) {
		*val = 0;
		return -EINVAL;
	}

	if (bank == DIAG_RO_BANK)
		adr += diag_rw_words;

	syscon->DIAG_CR = SYSC_DIAG_CR_ADR_W(adr);
	*val = syscon->DIAG_DAT;

	return 0;
}

int diag_write_word(uint32_t adr, uint32_t val)
{
	if (adr >= diag_rw_words)
		return -EINVAL;

	syscon->DIAG_DAT = val;
	syscon->DIAG_CR = SYSC_DIAG_CR_RW | SYSC_DIAG_CR_ADR_W(adr);

	return 0;
}

void net_rst(void)
{
	syscon->GPSR |= SYSC_GPSR_NET_RST;
}

int wdiag_set_valid(int enable)
{
	if (enable)
		syscon->WDIAG_CTRL |= SYSC_WDIAG_CTRL_DATA_VALID;
	if (!enable)
		syscon->WDIAG_CTRL &= ~SYSC_WDIAG_CTRL_DATA_VALID;
	return (int)(syscon->WDIAG_CTRL & SYSC_WDIAG_CTRL_DATA_VALID);
}

int wdiag_get_valid(void)
{
	if (syscon->WDIAG_CTRL & SYSC_WDIAG_CTRL_DATA_VALID)
		return 1;
	else
		return 0;
}

int wdiag_get_snapshot(void)
{
	if (syscon->WDIAG_CTRL & SYSC_WDIAG_CTRL_DATA_SNAPSHOT)
		return 1;
	else
		return 0;
}

void wdiags_write_servo_state(int wr_mode, uint8_t servostate, uint64_t mu,
			      uint64_t dms, int32_t asym, int32_t cko,
			      int32_t setp, int32_t ucnt)
{
	syscon->WDIAG_SSTAT   = wr_mode ? SYSC_WDIAG_SSTAT_WR_MODE:0;
	syscon->WDIAG_SSTAT  |= SYSC_WDIAG_SSTAT_SERVOSTATE_W(servostate);
	syscon->WDIAG_MU_MSB  = 0xFFFFFFFF & (mu>>32);
	syscon->WDIAG_MU_LSB  = 0xFFFFFFFF &  mu;
	syscon->WDIAG_DMS_MSB = 0xFFFFFFFF & (dms>>32);
	syscon->WDIAG_DMS_LSB = 0xFFFFFFFF &  dms;
	syscon->WDIAG_ASYM    = asym;
	syscon->WDIAG_CKO     = cko;
	syscon->WDIAG_SETP    = setp;
	syscon->WDIAG_UCNT    = ucnt;
}

void wdiags_write_port_state(int link, int locked)
{
	uint32_t val = 0;

	val  = link   ? SYSC_WDIAG_PSTAT_LINK   : 0;
	val |= locked ? SYSC_WDIAG_PSTAT_LOCKED : 0;
	syscon->WDIAG_PSTAT = val;
}

void wdiags_write_ptp_state(uint8_t ptpstate)
{
	syscon->WDIAG_PTPSTAT = SYSC_WDIAG_PTPSTAT_PTPSTATE_W(ptpstate);
}

void wdiags_write_aux_state(uint32_t aux_states)
{
	syscon->WDIAG_ASTAT = SYSC_WDIAG_ASTAT_AUX_W(aux_states);
}

void wdiags_write_cnts(uint32_t tx, uint32_t rx)
{
	syscon->WDIAG_TXFCNT = tx;
	syscon->WDIAG_RXFCNT = rx;
}

void wdiags_write_time(uint64_t sec, uint32_t nsec)
{
	syscon->WDIAG_SEC_MSB = 0xFFFFFFFF & (sec>>32);
	syscon->WDIAG_SEC_LSB = 0xFFFFFFFF &  sec;
	syscon->WDIAG_NS      = nsec;
}

void wdiags_write_temp(uint32_t temp)
{
	syscon->WDIAG_TEMP = temp;
}
