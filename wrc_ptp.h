#ifndef __WRC_PTP_H
#define __WRC_PTP_H

#define WRC_MODE_GM 0
#define WRC_MODE_MASTER 1
#define WRC_MODE_SLAVE 2

int wrc_ptp_init();
int wrc_ptp_set_mode(int mode);
int wrc_ptp_start();
int wrc_ptp_stop();
int wrc_ptp_update();

#endif
