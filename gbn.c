#include "gbn.h"

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

int timeoutflag=0;

state_t s;
static int last_ack = 0;

void fill_header(uint16_t type, unsigned int seqnum, gbnhdr *hdr) 
{
    memset(hdr->data, '\0', DATALEN);
    
    if(type/2==1)
        hdr->type=true;
    else
        hdr->type=false;

    if(type%2==1)
        hdr->subtype=true;
    else
        hdr->subtype=false;

    hdr->seqnum = seqnum;
}

void handleTimeout(int signal) 
{
    timeoutflag=1;
    printf("TIME_OUT_ARLM\n");
}

int Type(void * buf)
{
    gbnhdr* temp= (gbnhdr *)buf;
    int a=0;
    if(temp->type)
        a=2;
    else
        a=0;

    if(temp->subtype)
        a=a+1;

    // int a=(temp->type)*2+ temp->subtype;
    // printf("A=%d SEQUENCE NUMBER=%d\n",a,temp->seqnum);
    return a;
}

int gbn_socket(int domain, int type, int protocol) 
{
    srand((unsigned)time(0));
    s = *(state_t*)malloc(sizeof(s));
    s.seqnum = 1;
    s.window = 1;
    s.state = CLOSED;
    signal(SIGALRM,handleTimeout);
    siginterrupt(SIGALRM,1);
    return socket(domain, type, protocol);
}

int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen) 
{
    s.state = SYN_SENT;
    gbnhdr *syn = malloc(sizeof(*syn));
    gbnhdr *synAck = malloc(sizeof(*synAck));
    gbnhdr *dataAck = malloc(sizeof(*dataAck));
    fill_header(SYN, s.seqnum, syn);
    struct sockaddr from;
    socklen_t fromLen = sizeof(from);
    while (s.state != ESTABLISHED && s.state != CLOSED) 
    {
        // Send SYN
        if (s.state == SYN_SENT) 
        {
            int res = send_packet(sockfd, syn, sizeof(*syn), 0, server, socklen);
            if (res == -1) 
            {
                perror("Error sending SYN.");
                s.state = CLOSED;
                return -1;
            }
            printf("SYN sent.\n");
            alarm(TIMEOUT);
        } 
        else 
        {
            printf("Error in SYN, resetting state to CLOSED.\n");
            s.state = CLOSED;
            alarm(0);
            return -1;
        }

        // Receive ACK
        int t=recvfrom(sockfd, synAck, sizeof(*synAck), 0, &from, &fromLen); 
        if (t>= 0) 
        {
            if (Type(synAck) == SYNACK) 
            {
                printf("SYNACK received.\n");
                // s.seqnum = synAck->seqnum;
                s.addr = *server;
                fill_header(DATAACK, synAck->seqnum, dataAck);

                // send ack from client to server for three way handshake
                int res = send_packet(sockfd, dataAck, sizeof(*dataAck), 0, server,socklen);
                if (res == -1) 
                {
                    perror("Error sending DATAACK.");
                    s.state = CLOSED;
                    return -1;
                }
                printf("DATAACK sent.\n");
                s.state = ESTABLISHED;
                alarm(0);
            }
        } 
        else 
        {
            printf("Error receiving SYNACK.\n");
            s.state = CLOSED;
            return -1;

        }
    }
    free(syn);
    free(synAck);
    free(dataAck);

    if (s.state == ESTABLISHED) {
        printf("ESTABLISHED.\n");
        return 0;
    }
    return -1;
}

int gbn_accept(int sockfd, struct sockaddr *client, socklen_t *socklen)
{
    s.state = CLOSED;
    gbnhdr *syn = malloc(sizeof(*syn));
    gbnhdr *synAck = malloc(sizeof(*synAck));
    gbnhdr *dataAck = malloc(sizeof(*dataAck));
    fill_header(DATAACK, 0, dataAck);

    // printf("HELLO\n");
    
    while (s.state != ESTABLISHED) 
    {
        switch (s.state) 
        {
            case CLOSED:
                if (recvfrom(sockfd, syn, sizeof(*syn), 0, client, socklen) >= 0) 
                {
                    if (Type(syn) == SYN) 
                    {
                        printf("SYN recieved.\n");
                        s.state = SYN_RCVD;
                        s.seqnum = syn->seqnum + (unsigned int) 1;
                    }
                } 
                else 
                {
                    printf("Error receiving SYN.\n");
                    s.state = CLOSED;
                    break;
                }
                break;

            case SYN_RCVD:
               while(1)
               {
                    fill_header(SYNACK, s.seqnum, synAck);
                    int res = send_packet(sockfd, synAck, sizeof(*synAck), 0, client, *socklen);
                    if (res == -1) 
                    {
                        perror("Error sending SYNACK.\n");
                        s.state = CLOSED;
                        return -1;
                    }
                    printf("SYNACK sent.\n");
                    alarm(TIMEOUT);
                    break;
                }

                if (recvfrom(sockfd, dataAck, sizeof(*dataAck), 0, client, socklen) >= 0) 
                {
                    if (Type(dataAck) == DATAACK) 
                    {
                        printf("DATAACK received.\n");
                        s.state = ESTABLISHED;
                        s.addr = *client;
                        break;
                    }
                } 
                else 
                {    
                    printf("Error receiving DATAACK.\n");
                    s.state = CLOSED;
                    return -1;
                }

                break;
            default: break;
        }
    }
    free(syn);
    free(synAck);
    free(dataAck);
    if (s.state == ESTABLISHED) 
    {
        printf("ESTABLISHED.\n");
        alarm(0);
        return sockfd;
    }
    return -1;
}

int gbn_listen(int sockfd, int backlog) 
{
    return 0;
}

int gbn_bind(int sockfd, const struct sockaddr *server, socklen_t socklen)
{
    return bind(sockfd, server, socklen);
}

int gbn_close(int sockfd) 
{
    s.state = CLOSED;
    printf("CLOSED\n");
    return close(sockfd);
}

ssize_t gbn_send(int sockfd, const void *buf, size_t len, int flags) 
{
    printf("Start sending \n");
    gbnhdr *dataPacket = malloc(sizeof(*dataPacket));
    gbnhdr *dataAck = malloc(sizeof(*dataAck));
    struct sockaddr from;
    socklen_t fromLen = sizeof(from);
    socklen_t serverLen = sizeof(s.addr);
    int i, j;
    int free_window = MAXWINDOWSIZE;
    // int unack_packets = 0;
    last_ack = s.seqnum;
    // // printf("GSBUF: %s\n",(char *)buf);
    for (i = 0; i < len;) 
    {
        // for (j = 0; j < s.window; j++) 
        while(free_window || timeoutflag)
        {   
            // printf("1st while\n");
            bool over = false;
            if(timeoutflag){
                timeoutflag = 0;
                free_window = MAXWINDOWSIZE;
                // printf("TIWIUFBWHFBWFBWFBBWFBIUWBIUFIBUWFBIUWBFIUFBIUWBIUBIUFWBIUFBIUFWBIUF\n");
            }

            while(free_window){
                // // printf("2nd while\n");
                
                j = MAXWINDOWSIZE - free_window + 1;
                // printf("... 2nd while    %d, len: %d\n",i,len);

                if(i + (DATALEN) * (j - 1) >= len){
                    over = true;
                    break;
                }



                size_t data_length = min(len - i - (DATALEN) * (j - 1), DATALEN);
                fill_header(DATA, s.seqnum + (unsigned int) j, dataPacket);
                memcpy(dataPacket->data, buf + i + (DATALEN) * (j - 1), data_length);

                // // printf("... dp: %s\n", (dataPacket->data));

                int res = send_packet(sockfd, dataPacket, sizeof(*dataPacket), 0, &s.addr, serverLen);
                if (res == -1) 
                {
                    perror("Error sending data.\n");
                    s.state = CLOSED;
                    return -1;
                }
                else
                {
                    alarm(TIMEOUT);
                }
                printf("Data sent seqnum = %d\n", dataPacket->seqnum);
                free_window--;
            }
            if(over)break;
        }
        
        size_t ack_packets = 0;
        // printf("free_window=%d",free_window);
        if (recvfrom(sockfd, dataAck, sizeof(*dataAck), 0, &from, &fromLen) >= 0) 
        {
            // printf("while Data received. seqnum = %d, last_ack: %d\n", dataAck->seqnum, last_ack);

            if (Type(dataAck) == DATAACK ) 
            {
                printf("Data ACK received. seqnum = %d\n", dataAck->seqnum);

                free_window=free_window+(dataAck->seqnum-last_ack);

                // printf("RECV_FORM free_wind=%d  DATAACK=%d, LASTACK=%d\n",free_window, dataAck->seqnum, last_ack);
                last_ack=dataAck->seqnum; 
                s.seqnum = dataAck->seqnum;
                i += DATALEN;
                if (free_window==MAXWINDOWSIZE) 
                {
                    // printf("remove alarm free_window=%d\n",free_window);
                    alarm(0);//remove alarm
                }
                else 
                {
                    //reset alarm
                    printf("reset alarm\n");

                    alarm(TIMEOUT);
                }
            }
        } 
        else 
        {
            if (errno == EINTR) 
            {
               // printf("YP NIHAJAFJ\n");
            }
            else 
            {
                s.state = CLOSED;
                return -1;
            }
        }
    }
    free(dataPacket);
    free(dataAck);

    printf("EXITING GBN_SEND i=%d, LENGTH=%d\n",i,len);

    if (s.state == ESTABLISHED) 
    {
        return len;
    }
    return -1;
 }

ssize_t gbn_recv(int sockfd, void *buf, size_t len, int flags) 
{
    gbnhdr *dataPacket = malloc(sizeof(*dataPacket));
    gbnhdr *dataAck = malloc(sizeof(*dataAck));
    struct sockaddr from;
    socklen_t fromLen = sizeof(from);
    socklen_t remoteLen = sizeof(s.addr);
    int received = 0;
    int returnValue = 0;

    // signal(SIGALRM,handleTimeout);
    // LINUX
    struct timeval tv;
    tv.tv_sec = 50;  /* 30 Secs Timeout */
    tv.tv_usec = 0;  // Not init'ing this can cause strange errors
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,sizeof(struct timeval));


    while (s.state == ESTABLISHED && received == 0) 
    {

        printf("WAITING TO RECV FOR PACKET %d\n",s.seqnum);
        if (recvfrom(sockfd,dataPacket, sizeof(*dataPacket), 0, &from, &fromLen) >= 0) 
        {
            // printf("AFA\n");
            // printf("LASR PACKET%d\n",s.seqnum);
            if (Type(dataPacket) == DATA) 
            {
                if (dataPacket->seqnum == s.seqnum) 
                {
                printf("Data received. seqnum = %d\n", dataPacket->seqnum);
                    // printf("Data seqnum.\n");
                    fill_header(DATAACK, s.seqnum, dataAck);
                    s.seqnum = dataPacket->seqnum + (unsigned int) 1;
                    memcpy(buf, dataPacket->data, strlen(dataPacket->data));
                    
                    char *temp = (char *)buf;
                    temp[strlen(dataPacket->data)] = '\0';
                    buf = temp;
                    // // printf("\nBuf: %s\n", buf);
                    returnValue += sizeof(dataPacket->data);
                    received = 1;
                    returnValue += sizeof(dataPacket->data);
                    // printf("BYE\n");
                } 
                else 
                {
                    printf("Incorrect data \n");

                    fill_header(DATAACK, s.seqnum-1, dataAck);
                }

                if (send_packet(sockfd,dataAck, sizeof(*dataAck), 0, &s.addr, remoteLen) == -1) 
                {
                    printf("Error in sending data acknowledgment. %d \n", errno);
                    s.state = CLOSED;
                    break;
                } 
                else 
                {
                    printf("Data sent. seqnum = %d\n", dataAck->seqnum);
                }
            } 
        }
        else
        {
            perror("TIMEOUT\n");

            // printf("ERROR NO=%d, EINTR=%d\n",errno,EINTR);
        	if (errno != EINTR) 
            {
	            break;
            }

        }
    }
    free(dataPacket);
    free(dataAck);
    printf("WFW(WFI\n");
    return returnValue;
}

ssize_t send_packet(int  s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
	char *buffer = malloc(len);
	memcpy(buffer, buf, len);

	int retval = sendto(s, buffer, len, flags, to, tolen);
	free(buffer);
	return retval;
}