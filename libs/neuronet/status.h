/* status.h */

#ifndef __STATUS_H
#define __STATUS_H

#include <neuro/NEURO.h>

#include "common.h"

extern Status_State Status_GetStatus(const Status *sta);
extern char *Status_GetPacket(const Status *sta);
extern int Status_GetPacketLen(const Status *sta);
extern Slave *Status_GetSlave(const Status *sta);
extern Master *Status_GetMaster(const Status *sta);
/* purge all instances of the slave conn from the buffer msr */
extern void Status_PurgeSlave(Master *msr, Slave *conn);

extern void Status_Move(Status *from, Status *to);

/* add to an EBUF in master */
extern void Status_Add(Master *msr, Status_State state, char *data, int len, Slave *conn);
extern void Status_AddPriority(Master *msr, Status_State state, char *data, int len, Slave *conn);
extern void Status_Set(Status *sta, Status_State state, char *data, int len, Slave *conn);

extern Status *Status_Create();
extern void Status_Clear(Status *sta);
extern void Status_Destroy(Status *sta);

#endif /* NOT __STATUS_H */
