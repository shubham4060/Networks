#include "gbn.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // Internet family of protocols
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


int count = 0, bytes_sent = 0;
struct timeval tv,start;

int main(int argc, char *argv[])
{
	int sockfd;          /* socket file descriptor of the client            */
	int numRead;
	socklen_t socklen;	 /* length of the socket structure sockaddr         */
	char buf[DATALEN * N];   /* buffer to send packets                       */
	struct hostent *he;	 /* structure for resolving names into IP addresses */
	FILE *inputFile;     /* input file pointer                              */
	struct sockaddr_in server;

	int count = 0;

	socklen = sizeof(struct sockaddr);

	/*----- Checking arguments -----*/
	if (argc != 3){
		fprintf(stderr, "usage: sender <hostname> <port> <filename>\n");
		exit(-1);
	}

	/*----- Opening the input file -----*/
	if ((inputFile = fopen(argv[2], "rb")) == NULL){
		perror("fopen");
		exit(-1);
	}


	/*----- Opening the socket -----*/
	if ((sockfd = gbn_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
		perror("gbn_socket");
		exit(-1);
	}

		// printf("HELLOSOCKFD=%d\n",sockfd);


	/*--- Setting the server's parameters -----*/
	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr   = inet_addr("10.0.0.2");
	server.sin_port   = htons(atoi(argv[1]));

	/*----- Connecting to the server -----*/
	if (gbn_connect(sockfd, (struct sockaddr *)&server, socklen) == -1){
		perror("gbn_connect");
		printf("wrong\n" );
		exit(-1);
	}

	// ----- Reading from the file and sending it through the socket -----
	gettimeofday(&tv, NULL);
	start = tv;
	while ((numRead = fread(buf, 1, DATALEN * N, inputFile)) > 0){
		printf("here\n" );
		if (gbn_send(sockfd, buf, strlen(buf), 0) == -1){
			perror("gbn_send");
			exit(-1);
		}

		bytes_sent += strlen(buf);
		count++;
		gettimeofday(&tv, NULL);

	    printf("Starttime: %d: %d, End time: %d: %d\n",start.tv_sec, start.tv_usec,tv.tv_sec, tv.tv_usec);
        printf("Count: %d Bytes Sent: %d\n",count, bytes_sent);
	}


	// printf("SENDER GOING OFF DD D\n");
	// printf("Count: %d\n", count);
	/*----- Closing the socket -----*/
	if (gbn_close(sockfd) == -1){
		perror("gbn_close");
		exit(-1);
	}

	/*----- Closing the file -----*/
	if (fclose(inputFile) == EOF){
		perror("fclose");
		exit(-1);
	}

	return(0);
}
