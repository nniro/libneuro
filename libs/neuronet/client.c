/* client.c
 * Module : Client
 */

/*-------------------- Extern Headers Including --------------------*/
#ifndef WIN32
#include <sys/select.h> /* select */

#ifndef __USE_MISC
#define __USE_MISC /* to make inet_aton work */
#endif /* NOT __USE_MISC */
#include <arpa/inet.h> /* inet_aton inet_ntoa */
#include <sys/socket.h>
#include <netinet/in.h> /* htons */
#include <netdb.h> /* gethostbyname */
#include <fcntl.h> /* fcntl (control of the sockets) */

#else /* WIN32 */

#include <windows.h> /* (winsock functions) */
#define MSG_DONTWAIT 0

#endif /* WIN32 */

#include <string.h> /* memcpy */
#include <errno.h> /* errno (variable) */
#include <stdio.h> /* sprintf perror */

#include <neuro/NEURO.h>

/*-------------------- Local Headers Including ---------------------*/
#include <global.h>

#include "common.h"

#include "slave.h"
#include "master.h"
#include "status.h"
#include "lbuf.h"
#include "util.h"
#include "server.h"

/*-------------------- Main Module Header --------------------------*/
#include "client.h"


/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("netclient");

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

/*-------------------- Static Prototypes ---------------------------*/



/*-------------------- Static Functions ----------------------------*/

static void
clean_fragment_master(void *src)
{
	FRAGMENT_MASTER *tmp;

	tmp = (FRAGMENT_MASTER*)src;

	if (tmp)
	{
		if (tmp->data)
			free(tmp->data);

		Neuro_CleanLBuf(tmp->fragmented);
	}
}

static void
clean_packet_buffer(void *src)
{
	PACKET_BUFFER *tmp;

	tmp = (PACKET_BUFFER*)src;

	if (tmp)
	{
		free(tmp->data);
	}
}

/* FIFO -- returns the first element in its buffer and removes it from the buffer. */
static char *
pop_input_data(EBUF *input, u32 *len)
{
	FRAGMENT_MASTER *master;
	FRAGMENT_SLAVE *slave;
	char *output = NULL;

	master = Neuro_GiveEBuf(input, 0);

	if (Neuro_LBufIsEmpty(master->fragmented))
	{
		/* the buffer contains no elements, this means that the 
		 * packet is a whole, theres no chunks in it.
		 */

		*len = master->len;
		output = master->data;

		/* NOTE this is very dangerous, better be careful */
		master->data = NULL;

		return output;
	}

	/* get the first element */
	slave = Neuro_GiveLBuf(master->fragmented);

	*len = *slave->len;
	output = slave->data;

	Neuro_SCleanLBuf(master->fragmented, slave);

	return output;
}

static int
Socket_Send(int connection, Slave *slv, char *message, u32 len)
{
	/* int _err = 0; */

	if (connection == 0 || message == NULL || len == 0)
		return 0;

	if (slv->master->protocolType == SOCK_STREAM || slv->master->type == TYPE_CLIENT)
	{
#ifndef WIN32
		/* MSG_NOSIGNAL avoids the server to be killed with the
		 * signal SIGPIPE. With it, send returns -1 and this results
		 * in a clean disconnection of the faulty client.
		 */
		return send(connection, message, len, MSG_NOSIGNAL);
#else /* WIN32 */
		return send(connection, message, len, 0);
#endif /* WIN32 */
	}
	else if (slv->master->protocolType == SOCK_DGRAM && slv->master->type == TYPE_SERVER)
	{
		return sendto(connection, message, len, 0, (struct sockaddr*)&slv->c_address, slv->addrlen);
	}

	return 0;
}

static int
Socket_Recv(int connection, char **output)
{
	if (!output)
		return 0;

	if (*output)
		free(*output);

	*output = calloc(1, MAX_PACKET_SIZE * INPUT_PACKET_BUFFERING);

	/* NEURO_TRACE("pipe available and recv data", NULL); */
	return recv(connection, *output, MAX_PACKET_SIZE * INPUT_PACKET_BUFFERING, 0);
}

/*-------------------- Global Functions ----------------------------*/

int
Client_Send(Slave *slv, const char *message, u32 len)
{
	Client *src;
	PACKET_BUFFER *tmp;
	int _err = 0;
	char *msg;
	int lenLoop = len;

	if (!slv || !message || len == 0)
	{
		WARN("Invalid argument values");
		return 1;
	}

	if (slv->type == TYPE_SERVER)
		return 1;

	src = slv->cType.client;

	msg = (char*)message;
	while (lenLoop > 0)
	{
		switch (Util_CheckPipeAvail(slv->socket, 1, 0, 5))
		{
			case 1:
			{
				_err = Socket_Send(slv->socket, slv, msg, lenLoop);

				if (_err == -1)
				{
					if (errno == EAGAIN)
						continue;

					Master_RmUfds(slv->master, slv);
					Master_PushEvent(slv->master, slv, 8);
					WARN(Neuro_s("Pipe error... disconnecting client, errno = %d", errno));
					return 2;
				}
				else
				{
					if (_err < lenLoop)
					{
						TRACE(Neuro_s("sent a packet of size %d/%d errno = %d", _err, len, errno));
						/* Client_Send(slv, &message[_err], len - _err); */
						msg = &msg[_err];
						lenLoop -= _err;
						continue;
					}
					else
					{
						TRACE(Neuro_s("sent a full packet of size %d", _err));
						lenLoop = 0;
						break;
					}
				}
			}
			break;

			case -1:
			{
				return 1;
			}
			break;

			default:
			{
				continue;
			}
			break;
		}
	}

#if old
	/*if (len >= MAX_PACKET_SIZE)
	{
		NEURO_WARN("too big packet input into the buffer (%d)", len);
		return 1;
	}*/

	Neuro_AllocEBuf(src->output, sizeof(PACKET_BUFFER*), sizeof(PACKET_BUFFER));

	tmp = Neuro_GiveCurEBuf(src->output);

	if (!tmp)
	{
		ERROR("Memory allocation error");
		return 1;
	}

	if (slv->master->inclpacket_size)
	{
		/* we allocate the size of the data that we will send plus the size of an integer 
		 * which will be the size of the data itself at the beginning of the packet.
		 */
		tmp->data = calloc(1, len + sizeof(u32));

		/* we copy the data to the packet at the address right after the length of the packet. */
		memcpy(&tmp->data[sizeof(u32)], message, len);

		tmp->len = len + sizeof(u32);

		/* we copy the size of the packet at the beginning of the packet */
		memcpy(tmp->data, &tmp->len, sizeof(u32));

	}
	else
	{
		tmp->data = calloc(1, len);

		memcpy(tmp->data, message, len);

		tmp->len = len;
	}

	/* NEURO_TRACE("Buffered output packet for client at address %x", src); */

	tmp->arrow = tmp->data;
	tmp->remaining = tmp->len;

	{
		int _err = 0;

		_err = Client_PollSend(slv);

		if (_err == 1)
		{
			return 0;
		}

		if (_err == 2)
		{
			/* need to disconnect client */
		}

		if (_err == 3)
		{
			ERROR("Client_PollSend raised an error");
			return 1;
		}
	}

	Master_PushEvent(slv->master, slv, 2);
#endif /* old */

	return 0;
}

void
Client_PopData(Slave *slv)
{
	char *packet_received = NULL;
	u32 plen = 0;
	Client *client;

	if (!slv || slv->type == TYPE_SERVER)
		return;

	client = slv->cType.client;

	if (!Neuro_EBufIsEmpty(client->input))
	{
		packet_received = pop_input_data(client->input, &plen);

		if (!packet_received)
		{
			ERROR(Neuro_s("packet_received is NULL len %d", plen));

			/* Status_Set(slv->master->status, State_NoData, NULL, 0, NULL); */
			/* return (slv->master->status); */

			return;
		}

		if (slv->master->inclpacket_size == 1)
		{
			/* we need to strip the beginning size integer 
			 * from the output.
			 */
			packet_received = &packet_received[sizeof(u32)];
		}

		TRACE("Packet wrapped and ready for the server");
		Status_Add(slv->master, State_DataAvail, packet_received, plen, slv);

		Util_SCleanEBuf(client->input, Neuro_GiveEBuf(client->input, 0));
	}
	else
	{
		/* Status_Set(slv->master->status, State_NoData, NULL, 0, slv); */
	}

	/* we received a packet from the connection with a client so we reset 
	 * the idle time.
	 */
	client->idle_time = Neuro_GetTickCount();

	/* return (slv->master->status); */
}

void
Client_Disconnect(Slave *slv)
{
	if (!slv)
		return;

	TRACE(Neuro_s("Disconnecting"));
		
	/* 
	 * this flags the client to disconnect immediately 
	 * see master.c the function Master_Poll
	 */
	Master_SetQuitFlag(slv->master);
}

char *
Client_GetIP(Slave *slv)
{
	if (!slv)
		return NULL;

	if (slv->type == TYPE_SERVER)
		return NULL;

	return inet_ntoa(slv->c_address.sin_addr);
}

void
Client_SetTimeOut(Slave *slv, t_tick ts)
{
	if (!slv)
		return;

	if (slv->type == TYPE_SERVER)
		return;

       slv->cType.client->timeout = ts;
}

/*-------------------- Poll ----------------------------------------*/

/* 
 * return values :
 * 0 = A new packet was successfully processed
 * 1 = Requiring disconnection of the client, probably due to a lost connection
 * 2 = A fatal error happened
 */
int
Client_PollRead(Slave *slv)
{
	char *rbuffer = NULL;
#ifndef WIN32
	ssize_t rbuflen = 0;
#else /* WIN32 */
	int rbuflen = 0;
#endif /* WIN32 */
	Client *client;

	if (!slv)
		return 2;

	client = slv->cType.client;

	switch (Client_Poll(client))
	{
		case 1:
		{
			return 1;
		}
		break;

		case 2:
		{
			return 2;
		}
		break;
	}

	/* we attempt to recieve data from the connection */
	rbuflen = Socket_Recv(slv->socket, &rbuffer);

	TRACE(Neuro_s("Recieved a packet of len %d", rbuflen));

	/* the connection with the client just ended, we close it up */
	if (rbuflen == 0 || rbuflen == -1)
	{
		free(rbuffer);

		if (rbuflen == 0)
			TRACE("Connection lost");
		else
			TRACE("Pipe error");

		return 1;
	}

#if old
	/* we process packets in our input buffer */
	if (Packet_Process_Input(client) == 1)
		return 2; /* the client buffer might have been freed so we bail out */
#endif /* old */

	/* we received something */
	if (rbuflen > 0)
	{
		if (Util_Buffer_Recv_Data(slv, rbuffer, rbuflen) == 1)
			return 2;
	}

	/* NEURO_TRACE("Packet received, size %d", rbuflen); */

	return 0;
}

/* 
 * return values :
 * 0 = No packet in the buffer
 * 1 = A packet from the buffer was successfully sent
 * 2 = Requiring disconnection of the client, probably due to a lost connection
 * 3 = A fatal error happened
 */
int
Client_PollSend(Slave *slv)
{
	Client *client;

	if (!slv)
		return 3;

	client = slv->cType.client;

	switch (Client_Poll(client))
	{
		case 1:
		{
			return 2;
		}
		break;

		case 2:
		{
			return 3;
		}
		break;
	}

	/* NEURO_TRACE("Client Send section -- %d", Neuro_EBufIsEmpty(client->output)); */

	/* this code sends packets from the output buffer */
	while (!Neuro_EBufIsEmpty(client->output))
	{
		PACKET_BUFFER *buf;
		int _err = 0;

		buf = Neuro_GiveEBuf(client->output, 0);

		if (buf->len >= MAX_PACKET_SIZE)
		{
			WARN(Neuro_s("Trying to send a packet bigger than the limit! %d bytes", 
					buf->len));

			return 3;
		}

		/* TRACE(Neuro_s("Packet sent size %d", buf->remaining)); */
		/* FIFO : First in first out method */
		if ((_err = Socket_Send(slv->socket, slv, buf->arrow, buf->remaining)) == buf->remaining)
		{
			Util_SCleanEBuf(client->output, buf);			
		}
		else
		{

			if (_err == -1)
			{

				WARN("Pipe error... disconnecting client");

				return 2;
			}
			else
			{
				/* This code actually sets the remaining bytes that weren't sent
				 * on the last pass of Socket_Send() function.
				 *
				 * This will ensure that all the data are sent.
				 */
				if (_err < buf->remaining)
				{
					buf->remaining -= _err;

					buf->arrow = &buf->data[buf->len - buf->remaining];
				}
			}
		}

		return 1;
	}

	return 0;
}

/*
 * returns 0 on no errors
 * returns 1 if the client requires disconnection
 * returns 2 on errors
 */
int
Client_Poll(Client *client)
{
	if (!client)
		return 2;

	if(client->timeout != 0)
	{
		if (client->idle_time + client->timeout < Neuro_GetTickCount())
		{
			TRACE("Connection dropped due to timeout");
			return 1;
		}
	}

	return 0;
}


/*-------------------- Constructor Destructor ----------------------*/

void
Client_Init(Master *msr, Client *client)
{
	client->input = NULL;
	Neuro_CreateEBuf(&client->input);
	Neuro_SetcallbEBuf(client->input, clean_fragment_master);

	client->output = NULL;
	Neuro_CreateEBuf(&client->output);
	Neuro_SetcallbEBuf(client->output, clean_packet_buffer);
}

Client *
Client_Create(Master *msr)
{
	Client *output = NULL;

	output = malloc(sizeof(Client));

	Client_Init(msr, output);

	return output;
}

/* establish connection with a server */
Slave *
Client_Connect(Master *msr, const char *host, int port)
{
	struct hostent *hent;
	struct sockaddr_in address; /* client address */
	int _err = 0;
	int optval;
#ifndef WIN32
	socklen_t optlen = 0;
#else /* WIN32 */
	int optlen = 0;
#endif /* WIN32 */
	char rIP[16];
	struct timeval timeout_write;
	Client *client;
	Slave *output;
	int sock;

	if (!msr)
	{
		ERROR("Invalid NNET_MASTER given as input");
		return NULL;
	}

	TRACE(Neuro_s("Attempting to connect to host %s on port %d", host, port));
	TRACE(Neuro_s("host len : %d", strlen(host)));

	if (host == NULL)
		return NULL;
	if (port <= 0)
		return NULL;

	hent = gethostbyname(host);

	if (hent == NULL)
		return NULL;

	/* set how long we retry to see if we connected or not(used with select) */
	timeout_write.tv_sec = 4;
	timeout_write.tv_usec = 0;

	/* Debug_Val(0, "Connecting to %s:%d\n", host, port); */

	/* write the address we will connect to to a variable */

	sprintf(rIP, "%d.%d.%d.%d", (u8)hent->h_addr_list[0][0],
			(u8)hent->h_addr_list[0][1],
			(u8)hent->h_addr_list[0][2],
			(u8)hent->h_addr_list[0][3]);

	TRACE(Neuro_s("Connection attempt on IP : %s", rIP));

	/* feed address with the vital connection
	 * informations like IP, port and type of
	 * connection.
	 */
	address.sin_family = AF_INET;
#ifndef WIN32
	inet_aton(rIP, (struct in_addr*)&address.sin_addr.s_addr);
#else /* WIN32 */
	address.sin_addr.s_addr = inet_addr(rIP);
#endif /* WIN32 */
	/* memcpy(&address.sin_addr, &hent->h_addr, sizeof(address.sin_addr)); */
	address.sin_port = htons(port);

	sock = socket(AF_INET, msr->protocolType, 0);

	if (sock <= 0)
	{
		WARN("socket creation failed");
		return NULL;
	}

	/* set the socket to non block */
#ifndef WIN32
	fcntl(sock, F_SETFL, O_NONBLOCK);
#else /* WIN32 */
	{
		unsigned long nb = 1;
		if (ioctlsocket(sock, FIONBIO, &nb) == SOCKET_ERROR)
			return NULL;

		TRACE(Neuro_s("NON BLOCKED == %d", nb));
	}
#endif /* WIN32 */

	_err = 0;
	while (_err == 0)
	{
		_err = connect(sock, (struct sockaddr*)&address, sizeof(address));

#ifdef WIN32
		/* NEURO_TRACE("WSAGetLastError() == %d IsBlocking == %d\n", 
		   WSAGetLastError(), WSAIsBlocking()); */
#endif /* WIN32 */

		if ( _err == 1
#ifndef WIN32
				&& errno != EINPROGRESS && errno != EALREADY
#else /* WIN32 */
				&& errno != WSAEINPROGRESS && errno != WSAEALREADY
#endif /* WIN32 */
		   )
		{
			WARN(Neuro_s("could not connect to host with error %d", errno));

			return NULL;
		}
		else
			_err = 0;

		TRACE("Checking availability of the Pipe");
		_err = Util_CheckPipeAvail(sock, 1, 4, 0);
		if (_err == 0)
			continue;

		if (_err < 0)
			break;

		TRACE(Neuro_s("Epoll gave the _err : %d", _err));
	}

	if (_err == -1)
	{
		WARN(Neuro_s("an error happened with Util_CheckPipeAvail %d", errno));
		perror("Util_CheckPipeAvail()");
		return NULL;
	}

	if (_err == 0)
	{
		WARN("connection timed out");
		return NULL;
	}

	/* we check for any errors with the connection since 
	 * we are using non blocking.
	 */
	optlen = sizeof(optval);
	_err = getsockopt(sock, SOL_SOCKET, SO_ERROR, (int *)&optval, &optlen);
	if (_err == -1)
	{
		/* printf("problem with getsockopt detected\n"); */
		ERROR("Could not set the socket with getsockopt");
		return NULL;
	}
	/*else
	  {
	  printf("optval %d optlen %d\n", optval, optlen);
	  }*/

	if (optval > 0)
	{
		/* printf("could not connect to \"%s\"\n", host); */
		ERROR(Neuro_s("getsockopt gave us the optval %d -- optlen %d", optval, optlen));
		return NULL;
	}

	TRACE("Connection established successfully");

	client = Client_Create(msr);

	client->connection_start_time = Neuro_GetTickCount();
	client->idle_time = client->connection_start_time;
	client->timeout = 500;

	output = Slave_Create(msr, sock, 1, client, NULL);

	msr->slave = output;

	return output;
}

void
Client_Destroy(Client *clt)
{
	if (!clt)
	{
		ERROR("Invalid arguments");
		return;
	}

	Neuro_CleanEBuf(&clt->input);
	Neuro_CleanEBuf(&clt->output);

	free(clt);
}
