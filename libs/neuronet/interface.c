/* interface of the whole module */

/*-------------------- Extern Headers Including --------------------*/
#include <neuro/NEURO.h>

/*-------------------- Local Headers Including ---------------------*/
#include <global.h>
#include "common.h"

#include "slave.h"
#include "master.h"
#include "client.h"
#include "server.h"
#include "status.h"

/*-------------------- Main Module Header --------------------------*/


/*--------------------      Other       ----------------------------*/

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

/*-------------------- Static Prototypes ---------------------------*/



/*-------------------- Static Functions ----------------------------*/

/*-------------------- Global Functions ----------------------------*/

Status_State
NNet_GetStatus(const Status *sta)
{
	return Status_GetStatus(sta);
}

char *
NNet_GetPacket(const Status *sta)
{
	return Status_GetPacket(sta);
}

int
NNet_GetPacketLen(const Status *sta)
{
	return Status_GetPacketLen(sta);
}

Slave *
NNet_GetSlave(const Status *sta)
{
	return Status_GetSlave(sta);
}

Master *
NNet_GetMaster(const Status *sta)
{
	return Status_GetMaster(sta);
}

void
NNet_SetData(Slave *slv, void *ptr)
{
	Slave_SetData(slv, ptr);
}

void *
NNet_GetData(const Slave *slv)
{
	return Slave_GetData(slv);
}

void
NNet_SetTimeOut(Slave *slv, t_tick ts)
{
	Client_SetTimeOut(slv, ts);
}

void
NNet_SetDebugFilter(const char *filter)
{
	Neuro_SetDebugFilter(filter);
}

char *
NNet_GetIP(Slave *slv)
{
	return Client_GetIP(slv);
}

int
NNet_Send(Slave *src, const char *message, u32 len)
{
	return Client_Send(src, message, len);
}

int
NNet_SetSendPacketSize(Master *msr)
{
	return Master_SetSendPacketSize(msr);
}

void
NNet_SetProtocolType(Master *msr, PROTOCOL_TYPE protocolType)
{
	if (!msr)
		return;

	/* We don't allow the protocol type to be changed when master is connected */
	if (msr->slave)
		return;

	switch (protocolType)
	{
		case PROTO_TCP:
		{
			msr->protocolType = SOCK_STREAM;
		}
		break;

		case PROTO_UDP:
		{
			msr->protocolType = SOCK_DGRAM;
		}
		break;

		default:
		{
			msr->protocolType = SOCK_STREAM;
		}
		break;
	}
}

void
NNet_SetQuitFlag(Master *msr)
{
	Master_SetQuitFlag(msr);
}

void
NNet_DisconnectClient(Slave *client)
{
	Server_DisconnectClient(client);
}

void
NNet_SetResponderCB(Master *msr, void *customData, int (*cb)(void *customData, Status *status))
{
	if (msr && cb)
	{
		msr->callback = cb;
		msr->customData = customData;
	}
}

Slave *
NNet_Listen(Master *msr, const char *listen_ip, int port)
{
	return Server_Create(msr, listen_ip, port);
}

Slave *
NNet_Connect(Master *msr, const char *host, int port)
{
	return Client_Connect(msr, host, port);
}

/*-------------------- Poll ----------------------------------------*/
Status *
NNet_Poll(Master *msr)
{
	return Master_Poll(msr);
}

/*-------------------- Constructor Destructor ----------------------*/

Master *
NNet_Create(CONNECT_TYPE connection_type)
{
	return Master_Create(connection_type);
}

void
NNet_Destroy(Master *msr)
{
	Master_Destroy(msr);
}
