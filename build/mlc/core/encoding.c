#include "encoding.h"

char *encode8u(char *p, unsigned char c)
{
    *(unsigned char *)p++ = c;
    return p;
}

char *decode8u(char *p, unsigned char *c)
{
    *c = *(unsigned char *)p++;
    return p;
}

/* encode 16 bits unsigned int (lsb) */
char *encode16u(char *p, uint16_t w)
{
#if IWORDS_BIG_ENDIAN
    *(byte *)(p + 0) = (w & 255);
    *(byte *)(p + 1) = (w >> 8);
#else
    *(unsigned short *)(p) = w;
#endif
    p += 2;
    return p;
}

/* Decode 16 bits unsigned int (lsb) */
char *decode16u(char *p, uint16_t *w)
{
#if IWORDS_BIG_ENDIAN
    *w = *(const unsigned char *)(p + 1);
    *w = *(const unsigned char *)(p + 0) + (*w << 8);
#else
    *w = *(const unsigned short *)p;
#endif
    p += 2;
    return p;
}

/* encode 32 bits unsigned int (lsb) */
char *encode32u(char *p, uint32_t l)
{
#if IWORDS_BIG_ENDIAN
    *(unsigned char *)(p + 0) = (unsigned char)((l >> 0) & 0xff);
    *(unsigned char *)(p + 1) = (unsigned char)((l >> 8) & 0xff);
    *(unsigned char *)(p + 2) = (unsigned char)((l >> 16) & 0xff);
    *(unsigned char *)(p + 3) = (unsigned char)((l >> 24) & 0xff);
#else
    *(uint32_t *)p = l;
#endif
    p += 4;
    return p;
}

/* Decode 32 bits unsigned int (lsb) */
char *decode32u(char *p, uint32_t *l)
{
#if IWORDS_BIG_ENDIAN
    *l = *(const unsigned char *)(p + 3);
    *l = *(const unsigned char *)(p + 2) + (*l << 8);
    *l = *(const unsigned char *)(p + 1) + (*l << 8);
    *l = *(const unsigned char *)(p + 0) + (*l << 8);
#else
    *l = *(const uint32_t *)p;
#endif
    p += 4;
    return p;
}