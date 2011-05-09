/* lbuf.h */

#ifndef __LBUF_H
#define __LBUF_H

#include <neuro/NEURO.h>

typedef struct LBUF LBUF;

extern void Neuro_SetcallbLBuf(LBUF *eng, void (*callback)(void *src));

extern void Neuro_AllocLBuf(LBUF *eng, size_t sobj);
/* places a new element at the beginning of the buffer instead of at the end */
extern void Neuro_AllocStartLBuf(LBUF *eng, size_t sobj);

extern void *Neuro_GiveCurLBuf(LBUF *eng);

extern u32 Neuro_GiveLBufCount(LBUF *eng);

extern void Neuro_SCleanLBuf(LBUF *eng, void *object);

extern void *Neuro_GiveLBuf(LBUF *eng);

extern void *Neuro_GiveNextLBuf(LBUF *eng);

extern void Neuro_ResetLBuf(LBUF *eng);

extern u32 Neuro_LBufIsEmpty(LBUF *eng);


extern LBUF *Neuro_CreateLBuf(void);
extern void Neuro_CleanLBuf(LBUF *eng);

#endif /* NOT __LBUF_H */