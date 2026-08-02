#ifndef __INC_ETERBASE_RANDOM_H__
#define __INC_ETERBASE_RANDOM_H__

extern void				srandom(unsigned long seed);
extern unsigned long	random();
extern float			frandom(float flLow, float flHigh);
extern long				random_range(long from, long to);

#endif
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
