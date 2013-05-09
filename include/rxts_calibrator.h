/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __RXTS_CALIBRATOR_H
#define __RXTS_CALIBRATOR_H

void rxts_calibration_start();
int rxts_calibration_update(int *t24p_value);
int measure_t24p(uint32_t *value);
int calib_t24p(int mode, uint32_t *value);

#endif
