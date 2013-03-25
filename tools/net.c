/*
	net.c: Provides functions for TCP comms with BINDER oven
	by Edward Cree
	for Solarflare
	(C) 2012
*/

#include "net.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#define BINDER_PORT	"10001" // TCP port for BINDER over Lantronix XPort

int binder_connect(const char *address, FILE *errfp)
{
	int e, sockfd;
	struct addrinfo hints, *list, *res;
	char ip[INET_ADDRSTRLEN];
	memset(&hints, 0, sizeof(hints));
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_STREAM;
	if((e=getaddrinfo(address, BINDER_PORT, &hints, &list)))
	{
		if(errfp) fprintf(errfp, "getaddrinfo: %s\n", gai_strerror(e));
		return(-1);
	}
	for(res=list;res;res=res->ai_next)
	{
		if((sockfd=socket(res->ai_family, res->ai_socktype, res->ai_protocol))<0)
		{
			if(errfp)
			{
				inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ip, sizeof(ip));
				fprintf(errfp, "socket %s: %s\n", ip, strerror(errno));
			}
			continue;
		}
		if((e=fcntl(sockfd, F_SETFD, O_NONBLOCK))==-1)
		{
			if(errfp)
			{
				inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ip, sizeof(ip));
				fprintf(errfp, "fcntl %s: %s\n", ip, strerror(errno));
			}
			continue;
		}
		if((e=connect(sockfd, res->ai_addr, res->ai_addrlen))<0)
		{
			close(sockfd);
			if(errfp)
			{
				inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ip, sizeof(ip));
				fprintf(errfp, "connect %s: %s\n", ip, strerror(errno));
			}
			continue;
		}
		freeaddrinfo(list);
		return(sockfd);
	}
	freeaddrinfo(list);
	return(-1); // we didn't get any connections
}

int binder_send(int sockfd, const unsigned char *buf, size_t len)
{
	size_t i=0;
	ssize_t b;
	while(i<len)
	{
		if((b=send(sockfd, buf+i, len-i, MSG_NOSIGNAL))<0) return(-1); // MSG_NOSIGNAL prevents SIGPIPE
		if(!b)
			if(errno!=EINTR) return(-1);
		i+=b;
	}
	return(0);
}

ssize_t binder_recv(int sockfd, unsigned char *buf, size_t len)
{
	while(1)
	{
		ssize_t b=recv(sockfd, buf, len, 0);
		if(b<0) return(b);
		if(!b)
		{
			if(errno!=EINTR) return(-1);
		}
		else
			return(b);
	}
}
