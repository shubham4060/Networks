#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

//server address
#define ADDR "10.0.0.2"

int sockfd, portno;
int bytesReceived = 0;
char recvBuff[1000];
size_t n;
struct sockaddr_in serv_addr;
char buffer[512];

struct timeval tv,start;
    int count = 0,bytes_recv = 0, start_time;

void handler(int sig){
    printf("Starttime: %d, Endtime: %d\n",start_time, tv.tv_usec);
    printf("Count: %d, bytes_recv: %d\n",count, bytes_recv);
    return;
}    
void error(const char *msg)
{
    perror(msg);
    exit(0);
}

typedef struct p{
    char buff[512];
    int size;
} packet;

int main(int argc, char *argv[])
{
    portno = 10000;

    printf("Please enter the filename: ");
    bzero(buffer,512);
    scanf("%s",buffer);

    // signal(SIGALRM, handler);

    memset(recvBuff, '0', sizeof(recvBuff));
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ADDR);
    serv_addr.sin_port = htons(portno);

    socklen_t serv_len=sizeof(serv_addr);

    n = sendto(sockfd,buffer,strlen(buffer),0,(const struct sockaddr *) &serv_addr,serv_len);
    int fp;
    fp = open(buffer, O_WRONLY  | O_CREAT ,0777); 
 

    packet new_packet;
 
    gettimeofday(&tv, NULL);
    start = tv;
    start_time = tv.tv_usec;
 
    while( bytesReceived=recvfrom(sockfd ,&new_packet, sizeof(new_packet),0,(struct sockaddr *) &serv_addr,&serv_len) > 0)
    {
        printf("Bytes received %d\n",new_packet.size);    
        int res=0;
        res=write(fp,new_packet.buff,new_packet.size);
 
        if( new_packet.size < 511 ) break;
        count++;
        bytes_recv += new_packet.size;
 
        gettimeofday(&tv, NULL);
    
    printf("Starttime: %d: %d, End time: %d: %d\n",start.tv_sec, start.tv_usec,tv.tv_sec, tv.tv_usec);

    printf("Count: %d, Bytes Recieved: %d\n",count, bytes_recv);
  
    }
       
    printf("File %s successfully downloaded !\n",buffer);
    close(fp);
    close(sockfd);
    return 0;
}