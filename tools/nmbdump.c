/*
	nmbdump: Dump memory of a net-MODBus device
	by Edward Cree
	for Solarflare
	(C) 2012
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "net.h"
#include "modbus.h"
#include "mberr.h"

void hexdump(FILE *fp, const unsigned char *data, size_t len);

int main(int argc, char *argv[])
{
	uint16_t addr;
	size_t len;
	bool have_addr=false, have_len=false, verbose=false;
	const char *remote=NULL;
	for(int arg=1;arg<argc;arg++)
	{
		if(strcmp(argv[arg], "-h")==0)
		{
			fprintf(stderr, "Usage: ./nmbtest <remote> -a<addr> [-l<length>] [-v]\n\t<addr> and <val> are in HEX and count WORDS\n");
			return(0);
		}
		if(strcmp(argv[arg], "-v")==0)
		{
			verbose=true;
		}
		else if(strncmp(argv[arg], "-a", 2)==0)
		{
			unsigned int a;
			if(sscanf(argv[arg]+2, "%x", &a)!=1)
			{
				fprintf(stderr, "Bad -a `%s'\n", argv[arg]+2);
				return(2);
			}
			if(a&~0xFFFF) fprintf(stderr, "Warning: -a: truncating address to 16 bits\n");
			addr=a;
			have_addr=true;
		}
		else if(strncmp(argv[arg], "-l", 2)==0)
		{
			unsigned int l;
			if(sscanf(argv[arg]+2, "%x", &l)!=1)
			{
				fprintf(stderr, "Bad -l `%s'\n", argv[arg]+2);
				return(2);
			}
			len=l;
			have_len=true;
		}
		else
		{
			if(remote)
			{
				fprintf(stderr, "Error: more than one <remote> specified on command line\n");
				return(2);
			}
			remote=argv[arg];
		}
	}
	if(!remote)
	{
		fprintf(stderr, "No remote supplied\n");
		return(2);
	}
	if(!have_addr)
	{
		fprintf(stderr, "No address supplied (use -a<addr>)\n");
		return(2);
	}
	if(!have_len)
	{
		fprintf(stderr, "No length supplied (use -l<length>)\n");
		return(2);
	}
	if(!len)
	{
		fprintf(stderr, "Length of zero is invalid\n");
		return(2);
	}
	if(addr+len-1<addr)
	{
		fprintf(stderr, "Address would roll over.  Address + Length must be <= 0x10000\n");
		return(2);
	}
	int sockfd=binder_connect(remote, stderr);
	if(sockfd<0) return(3);
	for(uint16_t off=0;off<len;off++)
	{
		mb_msg m;
		int e;
		if((e=mb_ct_req_readn(&m, addr+off, 1)))
		{
			fprintf(stderr, "mb_ct_req_readn: error %d\n", e);
			close(sockfd);
			return(1);
		}
		if(verbose)
		{
			fprintf(stderr, "SEND:\n");
			hexdump(stderr, m.data, m.len);
		}
		if((e=binder_send(sockfd, m.data, m.len)))
		{
			perror("binder_send");
			close(sockfd);
			return(3);
		}
		m.len=0;
		fd_set master;
		FD_ZERO(&master);
		FD_SET(sockfd, &master);
		while(m.len<MB_MAXMSGLEN)
		{
			fd_set readfds=master;
			struct timeval timeout={.tv_sec=1, .tv_usec=0};
			if(select(sockfd+1, &readfds, NULL, NULL, &timeout)==-1)
			{
				perror("select");
				close(sockfd);
				return(3);
			}
			if(!FD_ISSET(sockfd, &readfds))
			{
				if(!(off&7))
				{
					if(off) putchar('\n');
					printf("%04x:", addr+off);
				}
				printf(" TIME");
				goto msgok;
			}
			ssize_t bytes=binder_recv(sockfd, m.data+m.len, MB_MAXMSGLEN-m.len);
			if(bytes<0)
			{
				perror("recv");
				close(sockfd);
				return(3);
			}
			if(bytes)
			{
				m.len+=bytes;
				if(verbose)
				{
					fprintf(stderr, "RECV:\n");
					hexdump(stderr, m.data, m.len);
				}
				size_t awords;
				uint16_t val;
				if((e=mb_pa_resp_readn(&m, 1, &awords, &val)))
				{
					if(e!=MB_EMSHORT)
					{
						fprintf(stderr, "mb_pa_resp_readn: error %d\n", e);
						if(!(off&7))
						{
							if(off) putchar('\n');
							printf("%04x:", addr+off);
						}
						printf(" MBER");
						goto msgok;
					}
					if(verbose)
					{
						fprintf(stderr, "MB_EMSHORT, trying for more data\n");
					}
				}
				else
				{
					if(!awords)
					{
						fprintf(stderr, "Device only responded with 0 words, expected 1\n");
						close(sockfd);
						return(1);
					}
					else
					{
						if(awords>1)
							fprintf(stderr, "Warning: device responded with %zu words, only asked for 1\n", awords);
						if(!(off&7))
						{
							if(off) putchar('\n');
							printf("%04x:", addr+off);
						}
						printf(" %04x", val);
					}
					goto msgok;
				}
			}
		}
		fprintf(stderr, "Ran out of buffer, and still couldn't read the message\n"); // This shouldn't happen for correctly-formed messages, as they should always fit in MB_MAXMSGLEN bytes
		close(sockfd);
		return(1);
		msgok:
		fflush(stdout);
	}
	putchar('\n');
	close(sockfd);
	return(0);
}


void hexdump(FILE *fp, const unsigned char *data, size_t len)
{
	for(size_t i=0;i<len;i++)
	{
		if(!(i&7))
		{
			if(i) fputc('\n', fp);
			fprintf(fp, "%04x:", i);
		}
		fprintf(fp, " %02x", data[i]);
	}
	fputc('\n', fp);
}
