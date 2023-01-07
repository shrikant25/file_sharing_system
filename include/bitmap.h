#ifndef _BITMAP_H
#define _BITMAP_H

#define BITMAP_SIZE 2 // in bytes

#ifndef _TGL_BT
#define _TGL_BT
void toggle_bit(int , char *);
#endif

#ifndef _ST_AL_BTS
#define _ST_AL_BTS
void set_all_bits(char *);
#endif

#ifndef _UNST_AL_BTS
#define _UNST_AL_BTS
void unset_all_bits(char *);
#endif

#endif