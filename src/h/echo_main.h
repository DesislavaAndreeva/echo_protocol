#ifndef _ECHO_MAIN_H_
#define _ECHO_MAIN_H_

#include <sys/socket.h>
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

#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define log_echo(format, argum...) ({fprintf(stderr," "format"\r\n",##argum);})

/*Default echo port is 7, if you use it execute the program as priviledged user;
  You can execute as unpriviledged user for numbers higher that 1024*/
#define ECHO_PORT_DEFAULT 7 
#define ECHO_TCP_BACKLOG 100
#define ECHO_BUFSIZE 1024
#define ECHO_MAX_MSG_SIZE 260 /*Extra 4 bytes just in case*/

//create an alias for int
typedef int ECHO_STATUS;	
#define ECHO_OK		0
#define ECHO_FAIL	1
#define ECHO_BAD_PARAM	2
#define ECHO_NO_MEM_ERR 3
#define ECHO_NOT_FOUND 4
#define ECHO_LISTEN_SOCK_ERR 5
#define ECHO_CLIENT_SOCK_ERR 6
#define ECHO_CLIENT_THREAD_ERR 7
#define ECHO_RCV_ERR 8
#define ECHO_OPEN_SOCK_ERR 9
#define ECHO_CONNECT_ERR 10
#define ECHO_SEND_ERR 11
#define ECHO_NETWORK_UNREACHABLE 12
#define ECHO_SET_SOCK_FLG_ERR 13
#define ECHO_BIND_ERR 14
#define ECHO_PTHREAD_ERR 15
#define ECHO_NO_ROUTE_TO_HOST 16

extern const char *arrErrors[];

typedef struct echoServersData_t
{
	int udpStatus;
	int tcpStatus;
	int udpSocket;
	int tcpSocket;
	int BytesRecv;
	int tcpMaxConnections;
	unsigned int iClientsCount;
}echoServersData;

typedef struct thread_data
{
	echoServersData* pData;
	int sock;	
}pthread_params;

typedef struct echoClientInstance_t
{
	int sockfd;
	int msgLen;
	int protocol;
	int waitTime;
	int sendBytes;
	int recvMsgLen;
	char message[ECHO_MAX_MSG_SIZE];
	char recvMesg[ECHO_MAX_MSG_SIZE];
	struct timeval startTime;
	struct timeval timeout;
	struct sockaddr_in servAddr;
	char lastEchoResponse[ECHO_BUFSIZE];
}echoClientGlobal_t;

typedef struct Echo_Global_Data
{
	echoServersData echoServersData;
} EchoGlobal_t;

void *echoTcpCallback(void *params);
void *echoTcpListener(void *psGlobal);
void *echoUdpCallback(void *client_socket);

ECHO_STATUS echoHandleErrors(int err);
ECHO_STATUS echoPrintHelp(char *szProgName);
ECHO_STATUS echod_SetShutdown (int iEchoProto);
ECHO_STATUS echoClientStart(char** arg_values);
ECHO_STATUS echoServersStart(int tcp_max_connection);
ECHO_STATUS echoSetSocket(EchoGlobal_t *pGlobal, int iEchoProto);
ECHO_STATUS echoServerStart(EchoGlobal_t *pGlobal, int iEchoProto);
ECHO_STATUS incomingConnections(EchoGlobal_t *pGlobal,int iEchoProto);
ECHO_STATUS echoGlobalInit(EchoGlobal_t** ppGlobal, int tcp_max_connection);
ECHO_STATUS echoServerCloseConnections(EchoGlobal_t *pGlobal, int iEchoProto);

#endif /* _ECHO_MAIN_H_ */
