#ifndef __WRC_PTP_H
#define __WRC_PTP_H

#define WRC_MODE_UNKNOWN 0
#define WRC_MODE_GM 1
#define WRC_MODE_MASTER 2
#define WRC_MODE_SLAVE 3
#define WRC_MODE_ABSCAL 4
extern int ptp_mode;

int wrc_ptp_init(void);
int wrc_ptp_set_mode(int mode);
int wrc_ptp_get_mode(void);
int wrc_ptp_start(void);
int wrc_ptp_stop(void);
int wrc_ptp_update(void);

#endif
