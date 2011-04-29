#ifndef __LOGD_H_INCLUDED__

//
// If want to disable log for this module, change this value to 0
//
#define __DEBUG_GENERIC_LOG__		0

#if (__DEBUG_GENERIC_LOG__)
#define logd(x...)  	do {printk(x);} while(0)
#else
#define logd(x...)		do {} while(0)
#endif

#endif

