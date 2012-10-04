#ifndef EEP43_H
#define EEP43_H

/* returns TRUE for success */
int Write43(int portnum, uchar * SerialNum, int page, uchar * page_buffer);
int ReadMem43(int portnum, uchar * SerialNum, int page, uchar * page_buffer);

#endif
