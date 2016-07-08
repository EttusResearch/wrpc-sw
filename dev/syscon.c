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

struct s_i2c_if i2c_if[2] = {
	{SYSC_GPSR_FMC_SCL, SYSC_GPSR_FMC_SDA},
	{SYSC_GPSR_SFP_SCL, SYSC_GPSR_SFP_SDA}
};

volatile struct SYSCON_WB *syscon;

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

//  timer_init(1);

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
