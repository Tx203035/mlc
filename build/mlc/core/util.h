#ifndef _MLC_CORE_UTIL_H_
#define _MLC_CORE_UTIL_H_

#include "core.h"

#define mlc_max(a,b) ( (a) > (b) ? (a) : (b))
#define mlc_min(a,b) ( (a) < (b) ? (a) : (b))

#define mlc_align(a) (((a) + 7) & ~7 )

#ifndef __offsetof
    #define __offsetof(t, f) ((char *)&(((t *)0)->f)-(char*)0)
#endif

#define mlc_offset_of(t,f) (__offsetof(t,f))

static inline int mlc_shift_1_n(unsigned int v){
    if(v<=1){
        return 0;
    }
    else
    {
        return 1 + mlc_shift_1_n(v>>1);
    }
}

MLC_API uint64_t mlc_clock64(void);
MLC_API uint64_t mlc_clock64_ms(void);


#endif