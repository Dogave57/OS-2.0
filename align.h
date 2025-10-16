#ifndef _ALIGN
#define _ALIGN
#define align_up(val, align)((val+align-1)& ~(align-1))
#endif
