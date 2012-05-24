/* MiniBone library. BUGGY CRAP CODE INTENDED FOR TESTING ONLY! */

#ifndef __MINIBONE_LIB_H
#define __MINIBONE_LIB_H

#include <stdint.h>

void *mbn_open(const char *if_name, uint8_t target_mac[]);
void mbn_writel(void *handle, uint32_t d, uint32_t a);
uint32_t mbn_readl(void *handle, uint32_t a);
void mbn_close(void *handle);


#endif
