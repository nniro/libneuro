/* network.c 
 */

/*-------------------- Extern Headers Including --------------------*/
#ifndef WIN32
#include <sys/select.h> /* select() */

#define __USE_MISC /* to make inet_aton() work */
#include <arpa/inet.h> /* inet_aton() inet_ntoa */
#include <sys/socket.h>
#include <netinet/in.h> /* htons() */
#include <netdb.h> /* gethostbyname() */

#else /* WIN32 */

#include <windows.h>
#define MSG_DONTWAIT 0

#endif /* WIN32 */

#include <unistd.h> /* close() */
#include <fcntl.h> /* fcntl() (control of the sockets) */
#include <string.h> /* memcpy() */
#include <errno.h> /* errno */
#include <stdio.h> /* sprintf() */

/*-------------------- Local Headers Including ---------------------*/
#include "neuro/global.h"
#include "neuro/NEURO.h"

/*-------------------- Main Module Header --------------------------*/
#include "neuro/nnet/network.h"

/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("netcore");

struct LISTEN_DATA
{
	u32 type; /* 0 is server and 1 is a client*/
	u32 socket; /* master socket for this listening service */
	u32 port;

	struct sockaddr_in saddress; /* server address */
	u32 addrlen;

	int (*callback)(CONNECT_DATA *conn, char *data, u32 len);

	EBUF *connections; /* contains CONNECT_DATA elements */
};

struct CONNECT_DATA
{
	u32 socket; /* specific socket for this connection */
	struct sockaddr_in c_address; /* client address */
	u32 addrlen;

	t_tick connection_start_time;
	t_tick idle_time; /* idle time... actually its the exact time we last recieved activity from the connection */
	t_tick timeout;

	EBUF *input; /* input buffer that contains FRAGMENT_MASTER elements */
	EBUF *output; /* output buffer that contains PACKET_BUFFER elements */
};

#define MAX_PACKET_SIZE 512
#define INPUT_PACKET_BUFFERING 20

typedef struct PACKET_BUFFER PACKET_BUFFER;

struct PACKET_BUFFER
{
	u32 len;
	char *data;
};


typedef struct FRAGMENT_MASTER FRAGMENT_MASTER;

/* Inputed packets are known to sometimes be recieved in more than one chunk at once.
 * This means that a single recieve by recv() could return more than one packet at
 * once per cycles. 
 *
 * Because of that behavior, we have to buffer the input packets and fragment the allocated
 * buffer into their respective packet sizes. THIS STRUCT IS TO CONTAIN SUCH PACKETS!
 *
 * Every packets start with an integer that is the size of the data in a packet.
 * We check to see if that value is of a valid range then proceed to fill this struct.
 *
 * It is important to note that the initial data pointer is the buffer itself which
 * contains more than one packet (or just one could happen too). The EBUF fragmented
 * buffer is then filled with pointers to the areas in the initial data pointer.
 * This method is meant to saves memory by a lot.
 *
 *
 * EXAMPLE :
 *
 *	 buffer len(24) :
 *	 ---------------------------------
 *	 |000|000|000|000|000|000|000|000|
 *	 ---------------------------------
 *
 *	 single packet len(3) :
 *	 -----
 *	 |000|
 *	 -----
 *
 *	 so in that buffer we have 8 packets that are in the same buffer... which we need to
 *	 fragment up manually.
 */
struct FRAGMENT_MASTER
{
	/* the length of the initial data */
	u32 len;
	/* the data buffer which could contain one or more actual packets. */
	char *data;

	EBUF *fragmented; /* contains the fragmented PACKET_BUFFER elements */
};

typedef struct FRAGMENT_SLAVE FRAGMENT_SLAVE;

/* no need to clean this struct type, it should have nothing allocated inside it */
struct FRAGMENT_SLAVE
{
	u32 *len;
	char *data;
};

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

/* the variable containing all the informations 
 * for the core network/local server
 */
EBUF *_greatBuffer; /* contains LISTEN_DATA elements */

static LISTEN_DATA *ACTIVE_LISTEN;

#ifdef WIN32
static WSADATA wsaData;
#endif /* WIN32 */

/*-------------------- Static Prototypes ---------------------------*/

static int Client_Send(int connection, char *message, u32 len);

static int Client_Recv(int connection, char **output);

static int CheckPipeAvail(int connection, int type, int timeout_sec, int timeout_usec);

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

		if (!Neuro_EBufIsEmpty(tmp->fragmented))
			Neuro_CleanEBuf(&tmp->fragmented);
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

static void
clean_listen_context(void *src)
{
	LISTEN_DATA *tmp;

	tmp = (LISTEN_DATA*)src;

	if (tmp)
	{
#ifdef WIN32
		closesocket(tmp->socket);
#else /* NOT WIN32 */
		close(tmp->socket);
#endif /* NOT WIN32 */

		Neuro_CleanEBuf(&tmp->connections);
	}
}

static void
clean_connection_context(void *src)
{
	CONNECT_DATA *tmp;

	tmp = (CONNECT_DATA*)src;

	if (tmp)
	{
#ifdef WIN32
		closesocket(tmp->socket);
#else /* NOT WIN32 */
		close(tmp->socket);
#endif /* NOT WIN32 */

		Neuro_CleanEBuf(&tmp->input);
		Neuro_CleanEBuf(&tmp->output);
	}
}

static void
Handle_Connections(LISTEN_DATA *parent)
{
	struct sockaddr_in connect_addr; /* server address */
#ifndef WIN32
	u32 addrlen = 0;
#else /* WIN32 */
	i32 addrlen = 0;
#endif /* WIN32 */
	i32 _err = 0;


	addrlen = sizeof(struct sockaddr_in);

	/* we check to see if theres new clients who want to join the party. */
	_err = accept(parent->socket, (struct sockaddr*)&connect_addr, &addrlen);

	/* if theres a new connection, we will add it to our buffer */
	if (_err != -1)
	{
		CONNECT_DATA *tmp;

		/* we have a new client that wishes to connect so we let it connect */

		Neuro_AllocEBuf(parent->connections, sizeof(CONNECT_DATA*), sizeof(CONNECT_DATA));

		tmp = Neuro_GiveCurEBuf(parent->connections);

		tmp->socket = _err;

		tmp->connection_start_time = Neuro_GetTickCount();
		tmp->idle_time = tmp->connection_start_time;

		memcpy(&tmp->c_address, &connect_addr, sizeof(struct sockaddr_in));
		tmp->addrlen = addrlen;

		Neuro_CreateEBuf(&tmp->input);
		Neuro_SetcallbEBuf(tmp->input, clean_fragment_master);

		Neuro_CreateEBuf(&tmp->output);
		Neuro_SetcallbEBuf(tmp->output, clean_packet_buffer);
	}
}

/* we delete the first element and reorder the elements back to their 
 * normal ordering.
 */
static void
clean_element_reorder(EBUF *input, void *element)
{
	u32 total = 0;
	u32 i = 0;
	void *temp;
	void *buf;

	if (Neuro_EBufIsEmpty(input))
		return;

	Neuro_SCleanEBuf(input, element);


	total = Neuro_GiveEBufCount(input);

	/* NEURO_TRACE("BEGIN LOOPING BUFFERED PACKETS", NULL); */
	while (i < total)
	{
		buf = Neuro_GiveEBuf(input, i);
		temp = Neuro_GiveEBuf(input, i + 1);

		/* NEURO_TRACE("BUFFERED PACKET LEN %d", buf->len); */
		if (temp == NULL)
			break;

		Neuro_SetEBuf(input, 
				Neuro_GiveEBufAddr(input, i + 1), buf);

		Neuro_SetEBuf(input, 
				Neuro_GiveEBufAddr(input, i), temp);
	
		i++;
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

	if (Neuro_EBufIsEmpty(master->fragmented))
	{
		/* the buffer contains no elements, this is an error */

		return NULL;
	}

	slave = Neuro_GiveEBuf(master->fragmented, 0);

	*len = *slave->len;
	output = slave->data;

	clean_element_reorder(master->fragmented, slave);

	return output;
}

/* this function processes the data that was recieved in the input (read) buffer 
 * and calls the parent's callback with the first packet that was recieved.
 */
static void
Buffer_Recv_Data(LISTEN_DATA *parent, CONNECT_DATA *client, char *rbuffer, u32 len)
{
	u32 *plen = NULL;
	char *packet_tosend = NULL;

	plen = (u32*)rbuffer;

	packet_tosend = (char*)&plen[1];

	/* fprintf(stderr, "PLEN %d  SIZE %d DATA %d\n", *plen, len, packet_tosend); */

	if (*plen > 0 && *plen <= len)
	{
		/* the packet is valid */

		/* buffering part... data is processed for buffering */

		if (*plen < len)
		{
			FRAGMENT_MASTER *cur;
			FRAGMENT_SLAVE *bufa;
			register u32 i = 0;

			/* we have a case where our buffer is containing more than one packet */



			Neuro_AllocEBuf(client->input, sizeof(FRAGMENT_MASTER*), sizeof(FRAGMENT_MASTER));

			cur = Neuro_GiveCurEBuf(client->input);

			
			Neuro_CreateEBuf(&cur->fragmented);



			while (i < len)
			{
				plen = (u32*)&rbuffer[i];

				Neuro_AllocEBuf(cur->fragmented, sizeof(FRAGMENT_SLAVE*), sizeof(FRAGMENT_SLAVE));

				bufa = Neuro_GiveCurEBuf(cur->fragmented);


				bufa->len = plen;
				bufa->data = (char*)&plen[1];



				i += *plen + sizeof(u32);
			}
		}
		





		/* packet handling part... data is processed for handling */

		if (!Neuro_EBufIsEmpty(client->input))
		{
			packet_tosend = pop_input_data(client->input, plen);
		}

		/* we recieved a packet from the connection with a client so we reset 
		 * the idle time.
		 */
		client->idle_time = Neuro_GetTickCount();

		ACTIVE_LISTEN = parent;
		switch ((parent->callback)(client, packet_tosend, *plen))
		{
			case 1:
			{
				/* we disconnect the client from the parent */
				Neuro_SCleanEBuf(parent->connections, client);
				
				/* free(rbuffer); */

				ACTIVE_LISTEN = NULL;
				return;
			}
			break;

			default:
			{
				FRAGMENT_MASTER *buf;

				if (!Neuro_EBufIsEmpty(client->input))
				{
					buf = Neuro_GiveEBuf(client->input, 0);

					if (Neuro_EBufIsEmpty(buf->fragmented))
						clean_element_reorder(client->input, buf);
				}
				else
					free(rbuffer);
			}
			break;

		}	
		ACTIVE_LISTEN = NULL;

	}
	else
	{
		/* the packet is not valid */
		NEURO_WARN("Invalid Packet was recieved. Suspected corrupt or not using the correct format for packets.", NULL);
		return;
	}

	/* fprintf(stderr, "END PROCESSING : PLEN %d  SIZE %d\n", *plen, len); */
}

static void
Handle_Clients(LISTEN_DATA *parent, CONNECT_DATA *client)
{
	char *rbuffer = NULL;
	ssize_t rbuflen;

	if (!client)
		return;

	if(client->timeout != 0)
	{
		if (client->idle_time + client->timeout < Neuro_GetTickCount())
		{
			Neuro_SCleanEBuf(parent->connections, client);
			NEURO_WARN("Connection dropped due to timeout", NULL);

			return;
		}
	}	

	/* we attempt to recieve data from the connection */
	rbuflen = Client_Recv(client->socket, &rbuffer);

	/* the connection with the client just ended, we close it up */
	if (rbuflen == 0)
	{

		free(rbuffer);

		NEURO_WARN("Connection lost", NULL);

		Neuro_SCleanEBuf(parent->connections, client);

		return;
	}

	/* we recieved something */
	if (rbuflen > 0)
	{	
		NEURO_TRACE("rbuflen %d --|", rbuflen);

		Buffer_Recv_Data(parent, client, rbuffer, rbuflen);

		return;

		/* we just got activity from the connection so we set 
		 * the timeout idle time variable to the current time.
		 */
		client->idle_time = Neuro_GetTickCount();

		/* we don't accept packets bigger than a certain size */
		if (rbuflen <= MAX_PACKET_SIZE)
		{
			ACTIVE_LISTEN = parent;
			switch ((parent->callback)(client, rbuffer, rbuflen))
			{
				case 1:
				{
					Neuro_SCleanEBuf(parent->connections, client);

					free(rbuffer);

					ACTIVE_LISTEN = NULL;

					return;
				}
				break;

				default:
				break;
			}
			ACTIVE_LISTEN = NULL;
		}
		else
		{
			NEURO_WARN("packet dropped because it was too big", NULL);
		}

		free(rbuffer);

		/* if we recieved a packet, we return and don't 
		 * send packets on this loop just in case we recieve
		 * more packets.
		 */
		return;
	}

	free(rbuffer);

	/* this code sends packets from the output buffer */
	if (!Neuro_EBufIsEmpty(client->output))
	{
		PACKET_BUFFER *buf;
		int _err = 0;

		buf = Neuro_GiveEBuf(client->output, 0);

		if (buf->len >= MAX_PACKET_SIZE)
		{
			NEURO_ERROR("Trying to send a packet bigger than the limit! %d bytes", 
					buf->len);
		}

		/* FIFO : First in first out method */
		if ((_err = Client_Send(client->socket, buf->data, buf->len)) == buf->len)
		{
			clean_element_reorder(client->output, buf);			
		}
		else
		{

			NEURO_WARN("We weren't able to send any data... disconnecting client", NULL);

			Neuro_SCleanEBuf(parent->connections, client);

			return;
		}
	}
}

static void
Handle_Listening(LISTEN_DATA *src)
{
	CONNECT_DATA *buf;
	u32 total = 0;

	/* this function needs to do 2 things :
	 * call a function to handle new connections
	 * and loop all those connections.
	 *
	 * the loop of all the connections first
	 * checks for recieved packets from
	 * each connections and then call the
	 * callback to handle the data.
	 * It will then (for each) send one packet
	 * from the output buffer for each of the
	 * connections.
	 *
	 */

	if (!src)
		return;

	/* checks for new connections and allocate accordingly 
	 * if theres any.
	 */
	if (src->type == 0)
		Handle_Connections(src);

	if (Neuro_EBufIsEmpty(src->connections))
		return;

	total = Neuro_GiveEBufCount(src->connections) + 1;

	while (total-- > 0)
	{
		buf = Neuro_GiveEBuf(src->connections, total);


		if (buf)
			Handle_Clients(src, buf);
	}
}

static int
Client_Send(int connection, char *message, u32 len)
{
	int _err = 0;

	if (connection == 0 || message == NULL || len == 0)
		return 0;

	_err = CheckPipeAvail(connection, 1, 0, 50);

	if (_err == 0)
		return 0;

	return send(connection, message, len, 0);
}

static int
Client_Recv(int connection, char **output)
{
	if (!output)
		return 0;

	if (*output)
		free(*output);

	if (CheckPipeAvail(connection, 0, 0, 50) == 0)
	{
		NEURO_TRACE("Recv pipe not available", NULL);
		return -1;
	}

	*output = calloc(1, MAX_PACKET_SIZE * INPUT_PACKET_BUFFERING);

	NEURO_TRACE("pipe available and recv data", NULL);
	return recv(connection, *output, MAX_PACKET_SIZE * INPUT_PACKET_BUFFERING, MSG_DONTWAIT);
}

/* returns 1 if pipe type [types]
 * is available else 0
 * types : 
 * 0 -- read
 * 1 -- write
 * 2 -- exception
 * 
 */
static int
CheckPipeAvail(int connection, int type, int timeout_sec, int timeout_usec)
{
	fd_set readfds, writefds, exceptfds;
	struct timeval timeout_write;
	int _err = 0;

	/* set how long we retry to see if we connected or not(used with select) 
	 * 
	 * 4 seconds wait/retry time
	 */
	timeout_write.tv_sec = timeout_sec;
	timeout_write.tv_usec = timeout_usec;

	if (type == 0)
	{
		FD_ZERO(&readfds);
		FD_SET(connection, &readfds);	
		_err = select(connection + 1, &readfds, NULL, NULL, &timeout_write);

		return (FD_ISSET(connection, &readfds) ? 1 : 0);
	}

	if (type == 1)
	{
		FD_ZERO(&writefds);
		FD_SET(connection, &writefds);	
		_err = select(connection + 1, NULL, &writefds, NULL, &timeout_write);

		return (FD_ISSET(connection, &writefds) ? 1 : 0);
	}

	if (type == 2)
	{
		FD_ZERO(&exceptfds);
		FD_SET(connection, &exceptfds);	
		_err = select(connection + 1, NULL, NULL, &exceptfds, &timeout_write);

		return (FD_ISSET(connection, &exceptfds) ? 1 : 0);
	}

	return 0;
}

static int
client_exist(LISTEN_DATA *src, CONNECT_DATA *c)
{
	CONNECT_DATA *buf;
	u32 total = 0;

	if (!src || !c)
	{
		NEURO_ERROR("Invalid argument", NULL);
		return -1;
	}

	if (!src->connections)
	{
		NEURO_ERROR("src->connections is NULL", NULL);
		return -1;
	}

	if (Neuro_EBufIsEmpty(src->connections))
	{
		NEURO_WARN("src->connections is empty", NULL);

		/* we return that the client don't exist */
		return 0;
	}

	total = Neuro_GiveEBufCount(src->connections) + 1;

	while (total-- > 0)
	{
		buf = Neuro_GiveEBuf(src->connections, total);

		if (buf == c)
			return 1;
	}

	return 0;
}

/*-------------------- Global Functions ----------------------------*/

int
NNet_SetTimeout(CONNECT_DATA *src, t_tick ts)
{
       if(!src)
               return 1;

       src->timeout = ts;

       return 0;
}

void
NNet_SetDebugFilter(char *filter)
{
	Neuro_SetDebugFilter(filter);
}

int
NNet_ClientExist(CONNECT_DATA *c)
{

	if (ACTIVE_LISTEN == NULL)
	{
		NEURO_ERROR("ACTIVE_LISTEN is NULL", NULL);
		return -1;
	}

	return client_exist(ACTIVE_LISTEN, c);
}


int
NNet_ClientExist2(LISTEN_DATA *l, CONNECT_DATA *c)
{

	if (l == NULL || c == NULL)
	{
		NEURO_ERROR("Invalid argument", NULL);
		return -1;
	}

	return client_exist(l, c);
}

/* establish connection with a server */
int
NNet_Connect(LISTEN_DATA *src, char *host, int port, CONNECT_DATA **result)
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
	CONNECT_DATA *tmp = NULL;

	if (host == NULL)
		return 1;
	if (port <= 0)
		return 1;

	hent = gethostbyname(host);

	if (hent == NULL)
		return 1;

	/* set how long we retry to see if we connected or not(used with select) */
	timeout_write.tv_sec = 4;
	timeout_write.tv_usec = 0;

	/* Debug_Val(0, "Connecting to %s:%d\n", host, port); */

	/* write the address we will connect to to a variable */

	sprintf(rIP, "%d.%d.%d.%d", (u8)hent->h_addr_list[0][0],
		(u8)hent->h_addr_list[0][1],
		(u8)hent->h_addr_list[0][2],
		(u8)hent->h_addr_list[0][3]);

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

	/* open up a socket */
	Neuro_AllocEBuf(src->connections, sizeof(CONNECT_DATA*), sizeof(CONNECT_DATA));	
	tmp = Neuro_GiveCurEBuf(src->connections);

	tmp->input = NULL;
	Neuro_CreateEBuf(&tmp->input);
	Neuro_SetcallbEBuf(tmp->input, clean_fragment_master);

	tmp->output = NULL;
	Neuro_CreateEBuf(&tmp->output);
	Neuro_SetcallbEBuf(tmp->output, clean_packet_buffer);

	tmp->socket = socket(AF_INET, SOCK_STREAM, 0);
	if (tmp->socket <= 0)
	{
		NEURO_WARN("socket creation failed", NULL);
		return 2;
	}

	/* set the socket to non block */
#ifndef WIN32
	fcntl(tmp->socket, F_SETFL, O_NONBLOCK);
#else /* WIN32 */
	{
		unsigned long nb = 1;
		if (ioctlsocket(tmp->socket, FIONBIO, &nb) == SOCKET_ERROR)
			return 1;

		NEURO_TRACE("NON BLOCKED == %d", nb);
	}
#endif /* WIN32 */

	_err = 0;
	while (_err == 0)
	{
		_err = connect(tmp->socket, (struct sockaddr*)&address, sizeof(address));

#ifdef WIN32
		/* NEURO_TRACE("WSAGetLastError() == %d IsBlocking == %d\n", 
				WSAGetLastError(), WSAIsBlocking()); */
#endif /* WIN32 */

		if (_err == 0
#ifndef WIN32
			&& errno != EINPROGRESS && errno != EALREADY
#else /* WIN32 */
			&& errno != WSAEINPROGRESS && errno != WSAEALREADY
#endif /* WIN32 */
			)
		{
			NEURO_WARN("could not connect to host with error %d", errno);

			return 1;
		}
		else
			_err = 0;

		_err = CheckPipeAvail(tmp->socket, 1, 4, 0);
		if (_err == 0)
			break;
	}


	if (_err == -1)
	{
		NEURO_WARN("an error happened with select %d", errno);
		perror("select()");
		return 2;
	}

	if (_err == 0)
	{
		NEURO_WARN("connection timed out", NULL);
		return 1;
	}

	/* we check for any errors with the connection since 
	 * we are using non blocking.
	 */
	optlen = sizeof(optval);
	_err = getsockopt(tmp->socket, SOL_SOCKET, SO_ERROR, (char *)&optval, &optlen);
	if (_err == -1)
	{
		/* printf("problem with getsockopt detected\n"); */
		return 2;
	}
	/*else
	{
		printf("optval %d optlen %d\n", optval, optlen);
	}*/

	if (optval > 0)
	{
		/* printf("could not connect to \"%s\"\n", host); */
		return 1;
	}

	NEURO_TRACE("Connection established successfully", NULL);

	tmp->connection_start_time = Neuro_GetTickCount();
	tmp->idle_time = tmp->connection_start_time;
	tmp->timeout = 500;

	*result = tmp;

	return 0;
}


char *
NNet_GetIP(CONNECT_DATA *src)
{
	if (!src)
		return 0;

	return inet_ntoa(src->c_address.sin_addr);
}

int
NNet_Send(CONNECT_DATA *src, char *message, u32 len)
{
	PACKET_BUFFER *tmp;

	if (!src || !message || len == 0)
		return 1;

	if (len >= MAX_PACKET_SIZE)
	{
		NEURO_WARN("too big packet input into the buffer (%d)", len);
		return 1;
	}

	Neuro_AllocEBuf(src->output, sizeof(PACKET_BUFFER*), sizeof(PACKET_BUFFER));

	tmp = Neuro_GiveCurEBuf(src->output);

	if (!tmp)
		return 1;

	/* NEURO_TRACE("%d of DATA to send", len); */


	/* we allocate the size of the data that we will send plus the size of an integer 
	 * which will be the size of the data itself at the beginning of the packet.
	 */
	tmp->data = calloc(1, len + sizeof(u32));

	/* we copy the size of the packet at the beginning of the packet */
	memcpy(tmp->data, &len, sizeof(u32));

	/* we copy the data to the packet at the address right after the length of the packet. */
	memcpy(&tmp->data[sizeof(u32)], message, len);


	tmp->len = len + sizeof(u32);

	return 0;
}

int
NNet_Listen(LISTEN_DATA *src, int port)
{
	struct sockaddr_in saddress; /* server address */
	int _err = 0;
	unsigned int addrlen = 0;

	addrlen = sizeof(struct sockaddr_in);

	src->socket = socket(AF_INET, SOCK_STREAM, 0);

	if (src->socket <= 0)
	{
		NEURO_ERROR(0, "socket creation failed\n");
		return 1;
	}

	saddress.sin_family = AF_INET;
	saddress.sin_addr.s_addr = INADDR_ANY;
	saddress.sin_port = htons(port);

	_err = bind(src->socket, (struct sockaddr*)&saddress, sizeof(saddress));

#ifndef WIN32
	fcntl(src->socket, F_SETFL, O_NONBLOCK);
#else /* WIN32 */
	{
		unsigned long nb = 1;
		if (ioctlsocket(src->socket, FIONBIO, &nb) == SOCKET_ERROR)
			return 1;
	}
#endif /* WIN32 */

	if (_err == -1)
	{
		NEURO_ERROR("binding failed", NULL);
		close(src->socket);
		return 1;
	}

	_err = listen(src->socket, 2);
	if (_err == -1)
	{
		NEURO_ERROR("flagging the master socket as listening failed", NULL);
		close(src->socket);
		return 1;
	}

	return 0;
}

/*-------------------- Poll ----------------------------------------*/

int
NNet_Poll()
{
	LISTEN_DATA *buf;
	u32 total = 0;

	if (Neuro_EBufIsEmpty(_greatBuffer))
		return 1;

	total = Neuro_GiveEBufCount(_greatBuffer) + 1;

	while (total-- > 0)
	{
		buf = Neuro_GiveEBuf(_greatBuffer, total);

		if (buf->type == 1 && Neuro_EBufIsEmpty(buf->connections))
			return 1;

		Handle_Listening(buf);
	}

	return 0;
}

/*-------------------- Constructor Destructor ----------------------*/

/* see LISTEN_DATA for what to put in type */
LISTEN_DATA *
NNet_Create(int (*callback)(CONNECT_DATA *conn, char *data, u32 len), u32 type)
{
	LISTEN_DATA *output;

	if (!callback)
		return NULL;

	if (Neuro_EBufIsEmpty(_greatBuffer))
	{
		Neuro_CleanEBuf(&_greatBuffer); /* just in case */

		Neuro_CreateEBuf(&_greatBuffer);
		Neuro_SetcallbEBuf(_greatBuffer, clean_listen_context);
	}

	Neuro_AllocEBuf(_greatBuffer, sizeof(LISTEN_DATA*), sizeof(LISTEN_DATA));

	output = Neuro_GiveCurEBuf(_greatBuffer);

	if (type > 1)
		type = 1;

	output->type = type;

	output->callback = callback;


	Neuro_CreateEBuf(&output->connections);
	Neuro_SetcallbEBuf(output->connections, clean_connection_context);

	return output;
}

void
NNet_Destroy(LISTEN_DATA *src)
{
	if (!src)
		return;

	Neuro_SCleanEBuf(_greatBuffer, src);
}

int
NNet_Init()
{
#if WIN32
	int _err = 0;

	_err = WSAStartup(MAKEWORD(1, 1), &wsaData);

	if (_err < 0)
		return 1;
#endif /* WIN32 */

	return 0;
}

void
NNet_Clean()
{
	Neuro_CleanEBuf(&_greatBuffer);

#ifdef WIN32
	WSACleanup();
#endif /* WIN32 */
}
