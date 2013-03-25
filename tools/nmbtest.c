/*
	nmbtest: Exerciser / client for net-MODBus (BINDER's variant MODBus over TCP)
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
#include "bits.h"
#include "modbus.h"
#include "mberr.h"

void hexdump(FILE *fp, const unsigned char *data, size_t len);

int main(int argc, char *argv[])
{
	uint16_t addr, val;
	float floatval;
	bool have_addr=false, have_val=false, have_floatval=false, floating=false, verbose=false;
	const char *remote=NULL;
	for(int arg=1;arg<argc;arg++)
	{
		if(strcmp(argv[arg], "-h")==0)
		{
			fprintf(stderr, "Usage: ./nmbtest <remote> -a<addr> [-w<val>]\n\t<addr> and <val> are in HEX\n");
			return(0);
		}
		if(strcmp(argv[arg], "-v")==0)
		{
			verbose=true;
		}
		else if(strcmp(argv[arg], "-f")==0)
		{
			floating=true;
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
		else if(strncmp(argv[arg], "-w", 2)==0)
		{
			unsigned int v;
			if(sscanf(argv[arg]+2, "%x", &v)!=1)
			{
				fprintf(stderr, "Bad -w `%s'\n", argv[arg]+2);
				return(2);
			}
			if(v&~0xFFFF) fprintf(stderr, "Warning: -w: truncating value to 16 bits\n");
			val=v;
			have_val=true;
		}
		else if(strncmp(argv[arg], "-fw", 3)==0)
		{
			float v;
			if(sscanf(argv[arg]+3, "%f", &v)!=1)
			{
				fprintf(stderr, "Bad -fw `%s'\n", argv[arg]+3);
				return(2);
			}
			floatval=v;
			have_floatval=true;
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
	int sockfd=binder_connect(remote, stderr);
	if(sockfd<0) return(3);
	if(have_val)
	{
		if(floating)
		{
			fprintf(stderr, "Error: can't combine -w and -f\n");
			return(2);
		}
		mb_msg m;
		int e;
		if((e=mb_ct_req_write(&m, addr, val)))
		{
			fprintf(stderr, "mb_ct_req_write: error %d\n", e);
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
			if(select(sockfd+1, &readfds, NULL, NULL, NULL)==-1)
			{
				perror("select");
				close(sockfd);
				return(3);
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
				uint16_t aaddr, aval;
				if((e=mb_pa_resp_write(&m, &aaddr, &aval)))
				{
					if(e!=MB_EMSHORT)
					{
						fprintf(stderr, "mb_pa_resp_write: error %d\n", e);
						close(sockfd);
						return(1);
					}
					if(verbose)
					{
						fprintf(stderr, "MB_EMSHORT, trying for more data\n");
					}
				}
				else
				{
					printf("[%04x] = %04x\n", aaddr, aval);
					if(aaddr!=addr)
					{
						fprintf(stderr, "Error: address mismatch\n");
						fprintf(stderr, "CAUTION!  The oven may be in an unexpected state.\n");
						close(sockfd);
						return(1);
					}
					if(aval!=val)
					{
						fprintf(stderr, "Error: data mismatch\n");
						fprintf(stderr, "CAUTION!  The oven may be in an unexpected state.\n");
						close(sockfd);
						return(1);
					}
					close(sockfd);
					return(0);
				}
			}
		}
		fprintf(stderr, "Ran out of buffer, and still couldn't read the message\n"); // This shouldn't happen for correctly-formed messages, as they should always fit in MB_MAXMSGLEN bytes
		close(sockfd);
		return(1);
	}
	else if(have_floatval)
	{
		mb_msg m;
		int e;
		unsigned char buf[4];
		if((e=mb_write_float(buf, floatval)))
		{
		    fprintf(stderr, "mb_write_float: error %d\n", e);
			close(sockfd);
			return(1);
		}
	    uint16_t val[2];
	    val[0]=read_be16(buf);
	    val[1]=read_be16(buf+2);
	    if((e=mb_ct_req_writen(&m, addr, 2, val)))
		{
			fprintf(stderr, "mb_ct_req_writen: error %d\n", e);
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
			if(select(sockfd+1, &readfds, NULL, NULL, NULL)==-1)
			{
				perror("select");
				close(sockfd);
				return(3);
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
				uint16_t aaddr;
				size_t awords;
				if((e=mb_pa_resp_writen(&m, &aaddr, &awords)))
				{
					if(e!=MB_EMSHORT)
					{
						fprintf(stderr, "mb_pa_resp_write: error %d\n", e);
						close(sockfd);
						return(1);
					}
					if(verbose)
					{
						fprintf(stderr, "MB_EMSHORT, trying for more data\n");
					}
				}
				else
				{
					printf("Wrote %zu words to %04x\n", awords, aaddr);
					if(aaddr!=addr)
					{
						fprintf(stderr, "Error: address mismatch\n");
						fprintf(stderr, "CAUTION!  The oven may be in an unexpected state.\n");
						close(sockfd);
						return(1);
					}
					if(awords!=2)
					{
						fprintf(stderr, "Error: data mismatch\n");
						fprintf(stderr, "CAUTION!  The oven may be in an unexpected state.\n");
						close(sockfd);
						return(1);
					}
					close(sockfd);
					return(0);
				}
			}
		}
		fprintf(stderr, "Ran out of buffer, and still couldn't read the message\n"); // This shouldn't happen for correctly-formed messages, as they should always fit in MB_MAXMSGLEN bytes
		close(sockfd);
		return(1);
	}
	else
	{
		mb_msg m;
		int e;
		if((e=mb_ct_req_readn(&m, addr, floating?2:1)))
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
			if(select(sockfd+1, &readfds, NULL, NULL, NULL)==-1)
			{
				perror("select");
				close(sockfd);
				return(3);
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
				uint16_t val[2];
				if((e=mb_pa_resp_readn(&m, floating?2:1, &awords, val)))
				{
					if(e!=MB_EMSHORT)
					{
						fprintf(stderr, "mb_pa_resp_readn: error %d\n", e);
						close(sockfd);
						return(1);
					}
					if(verbose)
					{
						fprintf(stderr, "MB_EMSHORT, trying for more data\n");
					}
				}
				else
				{
					if(awords<(floating?2:1))
					{
						fprintf(stderr, "Device only responded with %zu words, asked for %zu\n", awords, floating?2:1);
						close(sockfd);
						return(1);
					}
					else
					{
						if(awords>(floating?2:1))
							fprintf(stderr, "Warning: device responded with %zu words, only asked for %zu\n", awords, floating?2:1);
						if(floating)
						{
							unsigned char buf[4];
							write_be16(buf, val[0]);
							write_be16(buf+2, val[1]);
							printf("[%04x.f] = %g\n", addr, mb_read_float(buf));
						}
						else
							printf("[%04x] = %04x\n", addr, val[0]);
						close(sockfd);
						return(0);
					}
				}
			}
		}
		fprintf(stderr, "Ran out of buffer, and still couldn't read the message\n"); // This shouldn't happen for correctly-formed messages, as they should always fit in MB_MAXMSGLEN bytes
		close(sockfd);
		return(1);
	}
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
