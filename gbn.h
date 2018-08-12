#ifndef _gbn_h
#define _gbn_h
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <stdbool.h>

/*----- Error variables -----*/
extern int h_errno;
extern int errno;

/*----- Protocol parameters -----*/

#define DATALEN   1024    /* length of the payload                       */
#define N         1024    /* Max number of packets a single call to gbn_send can process */
#define TIMEOUT      10

/*----- Packet types -----*/
#define SYN      0        /* Opens a connection                          */
#define SYNACK   1        /* Acknowledgement of the SYN packet           */
#define DATA     2        /* Data packets                                */
#define DATAACK  3        /* Acknowledgement of the DATA packet          */

#define min(a,b) ((a) < (b) ? a : b) /*used to calculate minimum */
#define MAXWINDOWSIZE 6

/*----- Go-Back-n packet format -----*/
typedef struct 
{
	bool  type; 
	bool  subtype;
	unsigned int seqnum;          /* sequence number of the packet              */
    char data[DATALEN];    /* pointer to the payload                     */
} __attribute__((packed)) gbnhdr;

typedef struct state_t {
	int state;
    int window;
    unsigned int seqnum;
    struct sockaddr addr;
} state_t;

enum {
	CLOSED=0,
	SYN_SENT,
	SYN_RCVD,
	ESTABLISHED,
};

extern state_t s;
struct sigaction timeout_action;

int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen);
int gbn_listen(int sockfd, int backlog);
int gbn_bind(int sockfd, const struct sockaddr *server, socklen_t socklen);
int gbn_socket(int domain, int type, int protocol);
int gbn_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int gbn_close(int sockfd);
void type(void * buf);
ssize_t gbn_send(int sockfd, const void *buf, size_t len, int flags);
ssize_t gbn_recv(int sockfd, void *buf, size_t len, int flags);

ssize_t  send_packet(int  s, const void *buf, size_t len, int flags,const struct sockaddr *to, socklen_t tolen);


#endif
