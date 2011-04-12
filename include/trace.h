//#ifndef __TRACE_H__
//#define __TRACE_H__

#define MSGS_PROTO   1  // PTPWRd/protocol.c 
#define MSGS_WRPROTO 1  // PTPWRd/wr_protocol.c 
#define MSGS_WRSERVO 1  // PTPWRd/dep/wr_servo.c
#define MSGS_MSG     0  // PTPWRd/dep/msg.c  PTPWRd/dep/startup.c
#define MSGS_NET     0  // PTPWRd/dep/net.c
#define MSGS_ERR     1
#define MSGS_BMC     0  // PTPWRd/bmc.c 
#define MSGS_PTPD    0  // PTPWRd/ptpd.c
#define MSGS_WRAPPER 1  // libposix/freestanding_wrapper.c
#define MSGS_DISPLAY 0  // libposix/freestanding_display.c
#define MSGS_DEV     1  // dev/*

#if MSGS_PROTO
  #define TRACE_PROTO(...)   mprintf("[PROTO  ]: " __VA_ARGS__)
#else
  #define TRACE_PROTO(...)
#endif

#if MSGS_WRPROTO
  #define TRACE_WRPROTO(...) mprintf("[WRPROTO]: " __VA_ARGS__)
#else
  #define TRACE_WRPROTO(...)
#endif

#if MSGS_WRSERVO
  #define TRACE_WRSERVO(...) mprintf("[WR SRV ]: " __VA_ARGS__)
#else
  #define TRACE_WRSERVO(...)
#endif

#if MSGS_MSG
  #define TRACE_MSG(...)     mprintf("[MSG    ]: " __VA_ARGS__)
#else
  #define TRACE_MSG(...)
#endif 

#if MSGS_NET
  #define TRACE_NET(...)     mprintf("[NET    ]: " __VA_ARGS__)
#else
  #define TRACE_NET(...)
#endif

#if MSGS_ERR
  #define TRACE_ERR(...)     mprintf("[ERROR  ]: " __VA_ARGS__)
#else
  #define TRACE_ERR(...)
#endif

#if MSGS_BMC
  #define TRACE_BMC(...)     mprintf("[BMC    ]: " __VA_ARGS__)
#else
  #define TRACE_BMC(...)
#endif

#if MSGS_PTPD
  #define TRACE_PTPD(...)    mprintf("[PTPD   ]: " __VA_ARGS__)
#else
  #define TRACE_PTPD(...) 
#endif

#if MSGS_WRAPPER
  #define TRACE_WRAP(...)    mprintf("[WRAPPER]: " __VA_ARGS__)
#else
  #define TRACE_WRAP(...) 
#endif

#if MSGS_DISPLAY
  #define TRACE_DISP(...)    mprintf("[DISPLAY]: " __VA_ARGS__)
#else
  #define TRACE_DISP(...)
#endif

#if MSGS_DEV
  #define TRACE_DEV(...)     mprintf("[DEV    ]: " __VA_ARGS__)
#else
  #define TRACE_DEV(...)
#endif


//#endif
