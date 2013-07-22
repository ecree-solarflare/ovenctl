/*
  net.c: Provides functions for TCP comms with BINDER oven
  Copyright Solarflare Communications Inc., 2012
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of Solarflare Communications Inc. nor the names of its
        contributors may be used to endorse or promote products derived from
        this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL SOLARFLARE COMMUNICATIONS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
