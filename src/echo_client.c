#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
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
#include "echo_main.h"

echoClientGlobal_t *pClientGlobal = NULL;

/*********************************************************************
* Function Name  : echoClientTimeout()
* Description    : Procedure that is executed when the message is not 
				   received back from the server due to timeout;
* Input          : NONE
* Return         : ECHO_STATUS to indicate error/success
* Logic          : Print appropriate error message and close the socket
***********************************************************************/
ECHO_STATUS echoClientTimeout()
{
	bzero(pClientGlobal->lastEchoResponse, sizeof pClientGlobal->lastEchoResponse);
	strcpy(pClientGlobal->lastEchoResponse, "Echo request timed out.");
	log_echo("%s", pClientGlobal->lastEchoResponse);
	close(pClientGlobal->sockfd);
	pClientGlobal->sockfd = -1;
	
	return ECHO_OK;
} 

/*************************************************************************
* Function Name  : echoClientReceive()
* Description    : Receive messages from echo tcp/udp server
* Input          : clData - pointer to global echo client structure
* Return         : ECHO_STATUS to indicate error/success
* Logic          : If the message is received succesfully check for 
				   inconsitencies, calculate the time it took to 
				   receive the message and generate appropriate massages;
**************************************************************************/
ECHO_STATUS echoClientReceive(echoClientGlobal_t* clData)
{
	socklen_t addrlen = sizeof(clData->servAddr);
	char recvBuffer[ECHO_BUFSIZE];
	struct timeval vResTime;
	struct timeval vEndTime;
	int numBytesRecv = 0;
	double resTime;

	bzero(recvBuffer, sizeof recvBuffer);
	numBytesRecv = 0;
	
	gettimeofday(&clData->startTime, NULL);
	
	/*Set the time of waiting to receive the message back, i.e timeout*/
	/*The setsockopt() function set the SO_RCVTIMEO option, at the SOL_SOCKET protocol level, 
	  to the clData->timeout value for the clData->sockfd socket*/
	if (setsockopt(clData->sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&clData->timeout, sizeof clData->timeout) < 0)
	{
		log_echo("setsockopt(SO_RCVTIMEO) failed errno %d", errno);
		return ECHO_SET_SOCK_FLG_ERR;
	}
	
	switch(clData->protocol)
	{
		case IPPROTO_TCP:
		    //The recv() call is used to receive messages from a socket. It is used to receive data on connection-oriented sockets (TCP)
			numBytesRecv = recv(clData->sockfd, recvBuffer, ECHO_BUFSIZE, 0);
			break;
		
		case IPPROTO_UDP:
		    //The recvfrom() call is used to receive messages from a socket. It is used to receive data on connectionless sockets (UDP)
			numBytesRecv = recvfrom(clData->sockfd, recvBuffer, ECHO_BUFSIZE, 0, (struct sockaddr *)&clData->servAddr, &addrlen );
			break;
	}
	
	switch(numBytesRecv)
	{
		/* These calls return the number of bytes received, or -1 if an error
           occurred. In the event of an error, errno is set to indicate the error.*/
		case -1: 
			if(errno == EAGAIN) //timeout
				echoClientTimeout();
				
			return ECHO_OK;
			break;
		
		 /*When a stream socket peer has performed an orderly shutdown, the
           return value will be 0 (the traditional "end-of-file" return).

		   Datagram sockets in various domains (e.g., the UNIX and Internet
		   domains) permit zero-length datagrams.  When such a datagram is
		   received, the return value is 0.

		   The value 0 may also be returned if the requested number of bytes to
		   receive from a stream socket was 0.*/
		case 0:
			break;
			
		default:
			if( strlen(recvBuffer) > strlen(clData->message) )
			{
				sprintf(clData->lastEchoResponse, 
				"Something went wrong! Recieved more data than expected. Sent '%s' but recieved '%s%s'.", 
				clData->message, recvBuffer);
				
				log_echo("%s", clData->lastEchoResponse);
				
				bzero(clData->message, sizeof(clData->message) );
				bzero(clData->recvMesg, sizeof(clData->recvMesg) );
				close(clData->sockfd);					
				return ECHO_OK;
			}
			
			recvBuffer[numBytesRecv] = '\0';
			strncpy(clData->recvMesg, recvBuffer, strlen(recvBuffer));
			clData->recvMesg[clData->msgLen] = '\0';
			clData->recvMsgLen = strlen(recvBuffer);
			break;
	}
	
	/*Calculate for how much time the message was received*/	
	gettimeofday(&vEndTime, NULL);
	vResTime.tv_sec = vEndTime.tv_sec - clData->startTime.tv_sec;
	
	if(clData->startTime.tv_usec > vEndTime.tv_usec)
	{
		vResTime.tv_sec--;
		vResTime.tv_usec = clData->startTime.tv_usec - vEndTime.tv_usec;
	}
	else
	{
		vResTime.tv_usec = vEndTime.tv_usec - clData->startTime.tv_usec;
	}
	
	resTime = (double) vResTime.tv_sec*1000 + (double)vResTime.tv_usec/1000;
	
	bzero(clData->lastEchoResponse, sizeof clData->lastEchoResponse);

	if( strlen(clData->recvMesg) != clData->msgLen || 0 != strncmp(clData->recvMesg, clData->message, strlen(clData->message)) )
	{
		sprintf(clData->lastEchoResponse, "Error: sent message '%s' with size %d, received message '%s' with size %d",
				clData->message, clData->msgLen, clData->recvMesg, strlen(clData->recvMesg) );
		
		log_echo("%s", clData->lastEchoResponse);
	}
	else
	{
		sprintf(clData->lastEchoResponse, "Message '%s' was received for %.17gms", clData->recvMesg, resTime);
		log_echo("%s", clData->lastEchoResponse);
	}
		
	bzero(clData->message, sizeof(clData->message) );
	bzero(clData->recvMesg, sizeof(clData->recvMesg) );
	close(clData->sockfd);	
	
	return ECHO_OK;
} 

/*These are some errors I've got during development/testing; 
  TODO: handle more common errors*/
ECHO_STATUS echoHandleErrors(int err)
{
	if(101 == err)
	   log_echo("%s", arrErrors[ECHO_NETWORK_UNREACHABLE]);
				
	if(113 == err)
		log_echo("%s", arrErrors[ECHO_NO_ROUTE_TO_HOST]);
}

/********************************************************************
* Function Name  : echoClientSend()
* Description    : Connect to server (in case of TCP) 
				   and send the massage;
* Input          : clData - pointer to global echo client structure;
* Return         : ECHO_STATUS to indicate error/success
* Logic          : Follows standart procedure - open socket, connect
				   to echo server for TCP and send the message;
*********************************************************************/
ECHO_STATUS echoClientSend(echoClientGlobal_t* clData)
{
	socklen_t addrlen = sizeof(clData->servAddr);
	int res = 0;
	
	switch(clData->protocol)
	{
		case IPPROTO_TCP:
			clData->sockfd = socket(AF_INET, SOCK_STREAM, 0);
			break;
		
		case IPPROTO_UDP:
			clData->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
			break;
	}
	
	if ( clData->sockfd < 0 )
	{
		log_echo("echoClientSend socket() failed errno %d", errno);
		return ECHO_OPEN_SOCK_ERR;
	}
	
	switch(clData->protocol)
	{
		case IPPROTO_TCP:
			//system call that connects the socket referred to by the file
			//descriptor sockfd to the address specified by servAddr.
			res = connect(clData->sockfd, (struct sockaddr*)&clData->servAddr, sizeof(clData->servAddr));
			if ( res != 0 )
			{
				sprintf(clData->lastEchoResponse, "Failed to connect echo tcp server!");
				log_echo("echoClientSend connect() failed, errno %d", errno);
				log_echo("%s", clData->lastEchoResponse);
				echoHandleErrors(errno);
				close(clData->sockfd);
				return ECHO_CONNECT_ERR;
			}
			
			/*The system calls send() is used to transmit a message to another socket. It is used only when 
			  the socket is in a connected state (so that the intended recipient is known - TCP).*/
			clData->sendBytes = send(clData->sockfd, clData->message, clData->msgLen, 0);
			//log_echo("echoClientSend send() %d", clData->sendBytes);
			
			if(clData->sendBytes < 0 )
			{
				log_echo("echoClientSend send() failed, errno %d", errno);
				log_echo("%s", clData->lastEchoResponse);
				echoHandleErrors(errno);
				close(clData->sockfd);
				return ECHO_SEND_ERR;
			}
			break;
			
		case IPPROTO_UDP:
		     //The system call sendto() is used to transmit a message to another UDP socket;
			if((clData->sendBytes = sendto(clData->sockfd, clData->message, (size_t)clData->msgLen, 0, (struct sockaddr *)&clData->servAddr, addrlen)) < 0 )
			{
				sprintf(clData->lastEchoResponse, "Failed to connect echo udp server!");
				log_echo("echoClientSend sendto() failed, errno %d", errno);
				log_echo("%s", clData->lastEchoResponse);
				echoHandleErrors(errno);
				close(clData->sockfd);		
				return ECHO_SEND_ERR;
			}
			break;
	}
	
	return ECHO_OK;
}

/*Allocate memory and initialize the global echo client structure*/
ECHO_STATUS echoClientGlobalInit(echoClientGlobal_t **ppGlobal) 
{
	echoClientGlobal_t *pClGlobal = NULL;
	
	if (NULL == (pClGlobal = malloc(sizeof(echoClientGlobal_t) )) )
	{
		log_echo ("Could not allocate memory for global client struct");
		return ECHO_NO_MEM_ERR;
	}
	
	bzero(pClGlobal, sizeof(echoClientGlobal_t) );
	
	pClGlobal->msgLen = 0;
	pClGlobal->waitTime = 0;
	pClGlobal->sockfd = -1;
	pClGlobal->protocol = 0;
	pClGlobal->sendBytes = 0;
	pClGlobal->recvMsgLen = 0;
	
	bzero(pClGlobal->message, sizeof(pClGlobal->message) );
	bzero(pClGlobal->recvMesg, sizeof(pClGlobal->recvMesg) );
	bzero(pClGlobal->lastEchoResponse, sizeof(pClGlobal->lastEchoResponse) );
			
	*ppGlobal = pClGlobal;
  
     return ECHO_OK;
} 

ECHO_STATUS echoClientStart(char** arg_values) 
{
	ECHO_STATUS iRet = 0;
	
	iRet = echoClientGlobalInit (&pClientGlobal);
	if (ECHO_OK != iRet)
	{
		log_echo("Could not initialize client struct!");
		return iRet;
	}
	
	/*2: ip of the server
	  3: protocol to use - TCP/UDP
	  4: message to send
	  5: timeout
	  */
	
	inet_aton(arg_values[2], &pClientGlobal->servAddr.sin_addr);
	pClientGlobal->servAddr.sin_family = AF_INET;
	pClientGlobal->servAddr.sin_port = htons(ECHO_PORT_DEFAULT);
		
	sscanf (arg_values[3], "%d", &pClientGlobal->protocol);
	sscanf (arg_values[5], "%d", &pClientGlobal->waitTime);
	pClientGlobal->timeout.tv_usec = pClientGlobal->waitTime;
	
	strncpy(pClientGlobal->message, arg_values[4], ECHO_MAX_MSG_SIZE);
	pClientGlobal->msgLen = strlen(pClientGlobal->message);
		
	iRet = echoClientSend(pClientGlobal);
	if (ECHO_OK != iRet)
	{
		log_echo("%s", arrErrors[iRet]);
		return iRet;
	}
	
	iRet = echoClientReceive(pClientGlobal);
	if (ECHO_OK != iRet)
	{
		log_echo("%s", arrErrors[iRet]);
		return iRet;
	}
}
