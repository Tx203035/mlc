#ifndef KCP_ENCODING_H
#define KCP_ENCODING_H
#include <stdint.h>
//---------------------------------------------------------------------
// WORD ORDER
//---------------------------------------------------------------------
#ifndef IWORDS_BIG_ENDIAN
    #ifdef _BIG_ENDIAN_
        #if _BIG_ENDIAN_
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif
    #ifndef IWORDS_BIG_ENDIAN
        #if defined(__hppa__) || \
            defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
            (defined(__MIPS__) && defined(__MIPSEB__)) || \
            defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
            defined(__sparc__) || defined(__powerpc__) || \
            defined(__mc68000__) || defined(__s390x__) || defined(__s390__)
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif
    #ifndef IWORDS_BIG_ENDIAN
        #define IWORDS_BIG_ENDIAN  0
    #endif
#endif

char *encode8u(char *p, unsigned char c);

char *decode8u(char *p, unsigned char *c);

char *encode16u(char *p, uint16_t w);

char *decode16u(char *p, uint16_t *w);

char *encode32u(char *p, uint32_t l);

char *decode32u(char *p, uint32_t *l);

#endif //KCP_ENCODING_H
