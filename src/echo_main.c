#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <getopt.h>
#include "echo_main.h"

/* pointer to global struct */
EchoGlobal_t *pGlobal = NULL;
pthread_mutex_t lock;

const char *arrErrors[] =
{
	"OK",
	"General fail!",
    "Bad parameter!",
    "Memory allocation error!",
    "Not found!",
    "Listen error!",
    "Client socket error!",
    "Client thread error!",
    "Receive message error!",
    "Client open socket error!",
    "Client connect error!",
    "Client send error!",
    "Network unreachable!",
    "Set socket flags error!",
    "Client bind error!",
    "Client pthread create error!"
	"No route to host!"
};

/*************************************************************************
* Function Name  : echoServersStart()
* Description    : Initilaize the global struct and start TCP/UDP servers
* Input          : tcp_max_connection - max tcp clients count that could  
				   be handled simultaneously
* Return         : ECHO_STATUS to indicate error/success
**************************************************************************/
ECHO_STATUS echoServersStart(int tcp_max_connection) 
{
	ECHO_STATUS iRet = 0;
	
	/*Needed to synchronize the changes on iClientsCount
    	between the different threads(clients)*/
	if (pthread_mutex_init(&lock, NULL) != 0)
    {
        log_echo("Mutex init failed\n");
        return ECHO_FAIL;
    }
    
	iRet = echoGlobalInit (&pGlobal, tcp_max_connection);
	if (ECHO_OK != iRet)
	{
		log_echo("Could not initialize globalInit - %s!", arrErrors[iRet]);
		return iRet;
	}
	
	/* start echo servers */
	if(pGlobal->echoServersData.tcpStatus)
	{
		iRet = echoServerStart(pGlobal, IPPROTO_TCP);
		if (ECHO_OK != iRet)
			{
				log_echo("Could not start echo tcp server - %s! \n", arrErrors[iRet]);
				return iRet;
			}
	}
	else
		log_echo("TCP server stopped ... ");
	
	if(pGlobal->echoServersData.udpStatus)
	{
		iRet = echoServerStart(pGlobal, IPPROTO_UDP);
		if (ECHO_OK != iRet)
			{
				log_echo("Could not start echo udp server - %s!", arrErrors[iRet]);
				return iRet;
			}
	}
	else
		log_echo("UDP server stopped ... ");
	
	
	return ECHO_OK;
}

/*Allocate memory and initialize the global echo servers structure*/
ECHO_STATUS echoGlobalInit(EchoGlobal_t **ppGlobal, int tcp_max_connection) 
{
	EchoGlobal_t *pGlobal = NULL;
	
	log_echo ("Initializing echo global structure ... ");
	if (NULL == (pGlobal = malloc(sizeof(EchoGlobal_t) )) )
	{
		log_echo ("Could not allocate memory for global struct");
		return ECHO_NO_MEM_ERR;
	}
	
	bzero(pGlobal, sizeof(EchoGlobal_t) );
	
	pGlobal->echoServersData.tcpSocket = -1;
	pGlobal->echoServersData.udpSocket = -1;
	pGlobal->echoServersData.tcpStatus = 1;
	pGlobal->echoServersData.udpStatus = 1;
	pGlobal->echoServersData.tcpMaxConnections = tcp_max_connection;
	
	*ppGlobal = pGlobal;
  
  return ECHO_OK;
} 

/***********************************************************************
* Function Name  : echoSetSocket()
* Description    : Prepare TCP/UDP servers 
* Input          : iEchoProto - type of the protocol (TCP/UDP)
				   pGlobal - reference to global echo servers structure
* Return         : ECHO_STATUS to indicate error/success
* Logic          : Open sockets, bind them to adress/port and in case of
				   TCP listen for incomming connections; 
************************************************************************/
ECHO_STATUS echoSetSocket(EchoGlobal_t *pGlobal, int iEchoProto)
{
	int sock = -1;
	int res = -1;
	int iEchoPort = ECHO_PORT_DEFAULT;
	struct sockaddr_in  stServerAddr;
	
	switch(iEchoProto)
	{
		case IPPROTO_TCP:
			if(pGlobal->echoServersData.tcpSocket != -1)
				return ECHO_FAIL;
			
			/*Open TCP socket*/
			sock = socket(PF_INET, SOCK_STREAM, 0);
			break;
			
		case IPPROTO_UDP:
			if(pGlobal->echoServersData.udpSocket != -1)
				return ECHO_FAIL;
			
			/*Open UDP socket*/
			sock = socket(PF_INET, SOCK_DGRAM, 0);
			break;
	}
	 
	if (sock == -1) 
	{
		log_echo("Cannot allocate new socket %d", errno);
		return ECHO_OPEN_SOCK_ERR;
	}
	
	/*By default, TCP sockets are in "blocking" mode. For example, when you call recv() to read from a stream, control isn't returned to your program until 
	at least one byte of data is read from the remote site. This process of waiting for data to appear is referred to as "blocking". The same is true for 
	the write() API, the connect() API, etc. When you run them, the connection "blocks" until the operation is complete. Its possible to set a descriptor 
	so that it is placed in "non-blocking" mode. When placed in non-blocking mode, you never wait for an operation to complete. This is an invaluable tool 
	if you need to switch between many different connected sockets, and want to ensure that none of them cause the program to "lock up."*/
	fcntl(sock, F_SETFL, O_NONBLOCK); 
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) /*The SO_REUSEADDR socket option allows a socket to forcibly 
																				  bind to a port in use by another socket (because we have both 
																				  TCP and UDP sockets using same port). */
	{
		log_echo("setsockopt(SO_REUSEADDR) failed errno %d", errno);
		return ECHO_SET_SOCK_FLG_ERR;
	}
	
	bzero(&stServerAddr, sizeof(struct sockaddr_in));
	stServerAddr.sin_family = AF_INET; /*Adress family of IPv4 addresses*/
	stServerAddr.sin_port = htons(iEchoPort);
	log_echo("\n server port %d \n", stServerAddr.sin_port);
	
	switch(iEchoProto)
	{
		case IPPROTO_TCP:
			/*INADDR_ANY is 0, When INADDR_ANY is specified in the bind call,
			  the socket will be bound to all local interfaces.*/
			stServerAddr.sin_addr.s_addr = htonl(0); 
			break;
			
		case IPPROTO_UDP:
			stServerAddr.sin_addr.s_addr = htonl(0);
			break;
	}
	
	/*assign the address specified by stServerAddr to the socket referred to by the file descriptor sock.*/
	if((res = bind(sock, (struct sockaddr *)&stServerAddr, sizeof(struct sockaddr_in))) != 0)
	{
		log_echo("Can not bind server to socket %d!",errno);
		close(sock);
		return ECHO_BIND_ERR;
	}

	switch(iEchoProto)
	{
		case IPPROTO_TCP:
			pGlobal->echoServersData.tcpSocket = sock;
			break;
			
		case IPPROTO_UDP:
			pGlobal->echoServersData.udpSocket = sock;
			break;
	}
	
	if(iEchoProto == IPPROTO_TCP)
	{
		/*  listen() marks the socket referred to by sock as a passive socket,
			that is, as a socket that will be used to accept incoming connection
			requests using accept*/
		if( listen(sock, ECHO_TCP_BACKLOG) < 0)
		{
			log_echo("Can not set server to listen %d!",errno);
			close(sock);
			return ECHO_LISTEN_SOCK_ERR;
		}
	}
	
	return ECHO_OK;
}

ECHO_STATUS echoServerStart(EchoGlobal_t *pGlobal, int iEchoProto)
{
	if(iEchoProto != IPPROTO_TCP && iEchoProto != IPPROTO_UDP)
		return ECHO_BAD_PARAM;

	echoServerCloseConnections(pGlobal, iEchoProto);
	if ( ECHO_OK != echoSetSocket(pGlobal, iEchoProto) )
		return ECHO_FAIL;
		
	return incomingConnections(pGlobal, iEchoProto);	
}

/*********************************************************************
* Function Name  : incomingConnections()
* Description    : Start echo tcp/udp server
* Input          : pGlobal - reference to global echo servers DB
				   iEchoProto - type of protocol, TCP or UDP
* Return         : ECHO_STATUS to indicate error/success
* Logic          : Create pthreads that will handle TCP/UDP connections;
				   It allows us to control multiple different/concurrent
				   flows of work that overlap in time;
***********************************************************************/
ECHO_STATUS incomingConnections(EchoGlobal_t *pGlobal, int iEchoProto)
{
	pthread_t thread_id;
	
	switch(iEchoProto)
	{
		case IPPROTO_TCP:
			log_echo("incomingConnections TCP SOCK  [%d] \n", pGlobal->echoServersData.tcpSocket);
			
			//The pthread_create() function starts a new thread in the calling process.
			//The new thread starts execution by invoking echoTcpListener(); pGlobal is passed as argument of echoTcpListener().
			if( pthread_create( &thread_id , NULL,  echoTcpListener, (void*)pGlobal) < 0)
			{
				perror("could not create thread - echoTcpListener!");
				return ECHO_PTHREAD_ERR;
			}
			
			log_echo("Socket client accepted  [%d] \n", pGlobal->echoServersData.tcpSocket);
			break;
			
		case IPPROTO_UDP:		
			log_echo("incomingConnections  UDP SOCK  [%d] \n", pGlobal->echoServersData.udpSocket);	
			
			if( pthread_create( &thread_id , NULL ,  echoUdpCallback, (void*) &pGlobal->echoServersData.udpSocket) < 0)
			{
				perror("could not create thread - echoUdpHandler!");
				return ECHO_PTHREAD_ERR;
			}
			
			log_echo("Socket client accepted  [%d] \n", pGlobal->echoServersData.udpSocket);
			pthread_join(thread_id, NULL);
			break;
	}
	
	return ECHO_OK;
}

/************************************************************************
* Function Name  : echoTcpListener()
* Description    : A function that will be executed by pthread; Accepts
				   new connections on listenSock and create new thread
				   for the new client; only tcpMaxConnections simultaneous
				   clients are allowed;
* Input          : psGlobal - pointer to global echo DB;
* Return         : ECHO_STATUS to indicate error/success
*************************************************************************/
void *echoTcpListener(void *psGlobal) 
{
	EchoGlobal_t *pGlobal = (EchoGlobal_t *)psGlobal;
	int listenSock = pGlobal->echoServersData.tcpSocket;
	int clientSock = 0;
	pthread_t thread_id;
	ECHO_STATUS ret = 0;
	
	if(listenSock < 0)
	{
		ret = ECHO_LISTEN_SOCK_ERR;
		pthread_exit(&ret);
	}
	
	log_echo("TCP server is listening to sock=[%d] \n", listenSock);	
	while(1)
	{		
		if(pGlobal->echoServersData.iClientsCount < pGlobal->echoServersData.tcpMaxConnections)
		{	
			//It extracts the first connection request on the queue of pending connections for the listening socket,
			//listenSock, creates a new connected socket, and returns a new file descriptor referring to that socket - clientSock;
			clientSock = accept(listenSock, (struct sockaddr*)NULL, NULL);
			
			if(clientSock != -1)
			{
				log_echo("New client accept %d... ", clientSock);
									
				//new pthread for clients
				pthread_mutex_lock(&lock);
				pGlobal->echoServersData.iClientsCount++;
				pthread_mutex_unlock(&lock);
				
				pthread_params *params = malloc(sizeof(pthread_params));
				
				params->pData = &pGlobal->echoServersData;
				params->sock = clientSock;
				
				if( pthread_create( &thread_id , NULL ,  echoTcpCallback , (void*)params ) < 0)
				{
					log_echo("could not create thread");
					pthread_mutex_lock(&lock);
					pGlobal->echoServersData.iClientsCount--;
					pthread_mutex_unlock(&lock);
					ret = ECHO_CLIENT_THREAD_ERR;
					pthread_exit(&ret);
				}	
				
				//Suspend execution of the calling thread until the target thread terminates;
				pthread_join(thread_id, NULL);	
			}		
		}				
	}
		
	log_echo("echoTcpListener end\n");
}

/***********************************************************************
* Function Name  : echoTcpCallback()
* Description    : A function that will be executed by pthread; Handles
				   separate client - receive the message and then send 
				   it back;
* Input          : pthread_par - structure that contains the new client 
				   socket, created by accept() and a reference to global 
				   server DB;
* Return         : ECHO_STATUS to indicate error/success
***********************************************************************/
void *echoTcpCallback(void* pthread_par)
{
	pthread_params *p = (pthread_params *)pthread_par;
	int newsockfd = p->sock;
	char recvBuffer[ECHO_BUFSIZE];
	int numBytesRecv = 0 ;
	ECHO_STATUS ret = 0;
	
	log_echo("echoTcpCallback  [%d] \n", newsockfd);
	
	bzero(recvBuffer, ECHO_BUFSIZE);
	
	//The recv() call is used to receive messages from a socket. It is used to receive data on connection-oriented sockets (TCP)
	while((numBytesRecv = recv(newsockfd, recvBuffer, ECHO_BUFSIZE, MSG_DONTWAIT)) != -1)
	{		
		if(numBytesRecv == 0)
		{
			log_echo("recv from socket %d, numBytesRecv =%d errno %d\n", newsockfd, numBytesRecv, errno);
			close(newsockfd);
			break;
		}
		
		//The system calls send() is used to transmit a message to another socket. It is used only when the socket is in a connected
        //state (so that the intended recipient is known - TCP).
		numBytesRecv = send(newsockfd, recvBuffer, sizeof(recvBuffer), MSG_DONTWAIT);

		if (numBytesRecv < 0) 
			log_echo("ERROR writing to socket %d, errno %d\n", newsockfd, errno);
		
		bzero(recvBuffer, ECHO_BUFSIZE);
	}
	
	pthread_mutex_lock(&lock);
	pGlobal->echoServersData.iClientsCount--;
	pthread_mutex_unlock(&lock);

	p->pData = NULL;
	free(p);
	ret = ECHO_OK;
	pthread_exit(&ret);
}

/***********************************************************************
* Function Name  : echoUdpCallback()
* Description    : A function that will be executed by pthread; Handles
				   UDP clients - receive the message and send it back;
* Input          : client_socket - the UDP socket;
* Return         : ECHO_STATUS to indicate error/success
************************************************************************/
void *echoUdpCallback(void *client_socket)
{
	int newsockfd = *(int*)client_socket;
	struct sockaddr_in clientAddr;
	socklen_t addrLen = sizeof clientAddr;
	char recvBuffer[ECHO_BUFSIZE];
	int numBytesRecv = 0 ;
	int numBytesSent = 0;
	ECHO_STATUS ret;
	
	if(newsockfd < 0)
	{
		ret = ECHO_CLIENT_SOCK_ERR;
		pthread_exit(&ret);
	}
	
	log_echo("UDP server is listening to sock=[%d] \n", newsockfd);
	bzero(recvBuffer, sizeof recvBuffer);
	
	while (1) 
	{
		//The recvfrom() call is used to receive messages from a socket. It is used to receive data on connectionless sockets (UDP)
		numBytesRecv = recvfrom(newsockfd, recvBuffer, ECHO_BUFSIZE, 0, (struct sockaddr *) &clientAddr, &addrLen);
		
		if(numBytesRecv > 0)
		{
			log_echo("UDP recvfrom %d\n", numBytesRecv);
			//The system call sendto() is used to transmit a message to another socket (UDP).
			numBytesSent = sendto(newsockfd, recvBuffer, strlen(recvBuffer), 0, (struct sockaddr *) &clientAddr, addrLen);
			log_echo("UDP sendto %d %s\n", numBytesSent, recvBuffer);	
			bzero(recvBuffer, sizeof recvBuffer);
		}
		 
	}
		
	ret = ECHO_OK;
	pthread_exit(&ret);;
}

ECHO_STATUS echoServerCloseConnections(EchoGlobal_t *pGlobal, int iEchoProto)
{ 
	if(iEchoProto)
	{
		switch(iEchoProto)
		{
			case IPPROTO_TCP:
				close(pGlobal->echoServersData.tcpSocket);
				pGlobal->echoServersData.tcpSocket = -1;
				break;
				
			case IPPROTO_UDP:
				close(pGlobal->echoServersData.udpSocket);
				pGlobal->echoServersData.udpSocket = -1;
				break;
		}
	}
	
	return ECHO_OK;
}

int main( int argc, char** argv)
{
	int iOpt = 0;
	int tcp_max_connection = 0;
	struct option stLongOptions[] = { {"server", 0, 0, 1},
									  {"client", 0, 0, 2},
									  {0, 0, 0, 0} };
				
	while ( (iOpt = getopt_long( argc, argv, "sc", stLongOptions, NULL )) != -1 )
	{
		switch(iOpt)
		{
			case 1:
			case 's':
				if(argc == 3)
				{
					sscanf (argv[2], "%d", &tcp_max_connection);
					echoServersStart(tcp_max_connection);
					pthread_mutex_destroy(&lock);
				}
				break;
			
			case 2:
			case 'c':
				if(argc == 6)
					echoClientStart(argv);
				break;
				
			default:
				exit(1);
		}
	}
	
	return 1;
}
