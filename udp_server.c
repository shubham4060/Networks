#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

#define TRUE   1
#define FALSE  0
int opt = TRUE;
int max_sd;
int sockfd, newsockfd,portno,client_socket[30] , max_clients = 30 , activity, i , valread , sd;
size_t n;
socklen_t clilen;
char buffer[512];
struct sockaddr_in serv_addr, cli_addr;
struct timeval tv, start;
int count = 0, bytes_sent = 0, start_time;

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

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 10000;


    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 10000;


    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);
    bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr));
    clilen = sizeof(cli_addr);

    printf("Simple UDP server initiated\n");

    while(1)
    {

        printf("Ready for requests \n");

        bzero(buffer,512);
        clilen = sizeof(cli_addr);
        n = recvfrom(sockfd,buffer,512,0,(struct sockaddr *) &cli_addr, &clilen);

        printf("Here is the filename: %s\n",buffer);

        int pid;
        
        pid=fork();
        
        if (pid==0) {

             /* Open the file that we wish to transfer */
            int fp = open(buffer, O_RDONLY);
            /* Read data from file and send it */
            
            gettimeofday(&tv, NULL);
            start = tv;
            start_time = tv.tv_usec;
            while(1)
            {
                //creating new packet
                packet new_packet; 
                /* First read file in chunks of 512 bytes */
                bzero(new_packet.buff,512);
                int nread = read(fp,new_packet.buff,511);

                new_packet.size=nread;

                if(nread > 0)
                {
                    printf("Sending %d bytes\n",nread);
                     n = sendto(sockfd,&new_packet,sizeof(new_packet),0,(struct sockaddr *) &cli_addr, clilen);
                    bytes_sent += n;
                     count++;
                    if (n < 0) error("ERROR writing to socket");
                }

                if (nread < 511)
                {
                    printf("End of file\n");
                    close(fp);
                    break;
                }
                
            }
            printf("The file %s sent successfully !\n", buffer);gettimeofday(&tv, NULL);
            printf("Starttime: %d: %d, End time: %d: %d\n",start.tv_sec, start.tv_usec,tv.tv_sec, tv.tv_usec);
            printf("Count: %d Bytes Sent: %d\n",count, bytes_sent);
            printf("Ready for requests \n");
            exit(0);
         }
         else {}
    }
    close(sockfd);
    return 0; 
}