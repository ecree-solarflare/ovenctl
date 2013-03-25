/*
	net.h: Functions for TCP comms with BINDER oven
	by Edward Cree
	for Solarflare
	(C) 2012
*/

#ifndef HAVE_NET_H
#define HAVE_NET_H

#include <stdio.h>

int binder_connect(const char *address, FILE *errfp); // Connects to BINDER oven XPort.  Address may be IP or hostname.  Returns socket fd.  If errfp supplied, writes connection errors there
int binder_send(int sockfd, const unsigned char *buf, size_t len); // Tries to send all even in the face of short counts or EINTR.  Returns 0 on success, any other value on failure
ssize_t binder_recv(int sockfd, unsigned char *buf, size_t len); // Receives (handles EINTR).  Returns number of bytes read, or -1 on error

#endif // !def HAVE_NET_H
