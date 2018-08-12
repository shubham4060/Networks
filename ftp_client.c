#include <stdio.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <libgen.h>

#define BUF_SIZE 1024
#define SERVICE_READY 220
#define NEED_PASSWORD 331
#define LOGIN_SUCS 230
#define CONTROL_CLOSE 221
#define PATHNAME_CREATE 257
#define PASV_MODE 227
#define NO_SUCH_FILE 550
#define GET 1
#define PUT 2
#define PWD 3
#define DIR 4
#define CD 5
#define HELP 6
#define QUIT 7
#define CLS 8
#define CPWD 9
#define CLEAR 10

struct sockaddr_in server;
struct hostent* hent;
char user[20];
char pass[20];
int data_port;

void errorReport(char* err_info) {
    printf("# %s\n", err_info);
    exit(-1);
}

void sendCommand(int sock_fd, const char* cmd, const char* info) {
    char buf[BUF_SIZE] = {0};
    strcpy(buf, cmd);
    strcat(buf, info);
    strcat(buf, "\r\n");
    printf("COMMAND SENT= %s\n",buf);
    if (send(sock_fd, buf, strlen(buf), 0) < 0)
        errorReport("Send command error!");
}

int getReplyCode(int sockfd) {
    int r_code, bytes;
    char buf[BUF_SIZE] = {0}, nbuf[5] = {0};
    if ((bytes = read(sockfd, buf, BUF_SIZE - 2)) > 0) {
        r_code = atoi(buf);
        buf[bytes] = '\0';
        printf("%s", buf);
    }
    else
        return -1;
    if (buf[3] == '-') {
        char* newline = strchr(buf, '\n');
        if (*(newline+1) == '\0') {
            while ((bytes = read(sockfd, buf, BUF_SIZE - 2)) > 0) {
                buf[bytes] = '\0';
                printf("%s", buf);
                if (atoi(buf) == r_code)
                    break;
            }
        }
    }
    if (r_code == PASV_MODE) {
        char* begin = strrchr(buf, ',')+1;
        char* end = strrchr(buf, ')');
        strncpy(nbuf, begin, end - begin);
        nbuf[end-begin] = '\0';
        data_port = atoi(nbuf);
        buf[begin-1-buf] = '\0';
        end = begin - 1;
        begin = strrchr(buf, ',')+1;
        strncpy(nbuf, begin, end - begin);
        nbuf[end-begin] = '\0';
        data_port += 256 * atoi(nbuf);
    }

    return r_code;
}

int connectToHost(char* ip, char* pt) {
    int sockfd;
    int port = atoi(pt);
    if (port <= 0 || port >= 65536)
        errorReport("Invalid Port Number!");
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if ((server.sin_addr.s_addr = inet_addr(ip)) < 0) {
        if ((hent = gethostbyname(ip)) != 0)
            memcpy(&server.sin_addr, hent->h_addr, sizeof(&(server.sin_addr)));
        else
            errorReport("Invalid Host!");
    }
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        errorReport("Create Socket Error!");
    if (connect(sockfd, (struct sockaddr*)&server, sizeof(server)) < 0)
        errorReport("Cannot connect to server!");
    printf("Successfully connect to server: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    return sockfd;
}

int cmdToNum(char* cmd) {
    cmd[strlen(cmd)-1] = '\0';
    if (strncmp(cmd, "fget", 3) == 0)
        return GET;
    if (strncmp(cmd, "fput", 3) == 0)
        return PUT;
    if (strcmp(cmd, "servpwd") == 0)
        return PWD;
    if (strcmp(cmd, "servls") == 0)
        return DIR;
    if (strcmp(cmd, "servcd") == 0)
        return CD;
    if (strcmp(cmd, "clipwd") == 0)
        return CPWD;
    if (strcmp(cmd, "clils") == 0)
        return CLS;    
    if (strcmp(cmd, "?") == 0 || strcmp(cmd, "help") == 0)
        return HELP;
    if (strcmp(cmd, "quit") == 0)
        return QUIT;
    if (strcmp(cmd, "clear") == 0)
        return CLEAR;
    return -1;
}

void cmd_get(int sockfd, char* cmd) {
    int i = 0, data_sock, bytes;
    char filename[BUF_SIZE], buf[BUF_SIZE];
    while (i < strlen(cmd) && cmd[i] != ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    while (i < strlen(cmd) && cmd[i] == ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    strncpy(filename, cmd+i, strlen(cmd+i)+1);

    sendCommand(sockfd, "TYPE ", "I");
    getReplyCode(sockfd);
    sendCommand(sockfd, "PASV", "");
    if (getReplyCode(sockfd) != PASV_MODE) {
        printf("Error!\n");
        return;
    }
    server.sin_port = htons(data_port);
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        errorReport("Create socket error!");

    if (connect(data_sock, (struct sockaddr*)&server, sizeof(server)) < 0)
        errorReport("Cannot connect to server!");
    printf("Data connection successfully: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    sendCommand(sockfd, "RETR ", filename);
    if (getReplyCode(sockfd) == NO_SUCH_FILE) {
        close(sockfd);
        return;
    }

    FILE* dst_file;
    if ((dst_file = fopen(filename, "wb")) == NULL) {
        printf("Error!");
        close(sockfd);
        return;
    }
    while ((bytes = read(data_sock, buf, BUF_SIZE)) > 0)
        fwrite(buf, 1, bytes, dst_file);

    close(data_sock);
    getReplyCode(sockfd);
    fclose(dst_file);
}

void cmd_put(int sockfd, char* cmd) {
    int i = 0, data_sock, bytes;
    char filename[BUF_SIZE], buf[BUF_SIZE];
    while (i < strlen(cmd) && cmd[i] != ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    while (i < strlen(cmd) && cmd[i] == ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    strncpy(filename, cmd+i, strlen(cmd+i)+1);

    sendCommand(sockfd, "PASV", "");
    if (getReplyCode(sockfd) != PASV_MODE) {
        printf("Error!");
        return;
    }
    FILE* src_file;
    if ((src_file = fopen(filename, "rb")) == NULL) {
        printf("Error!");
        return;
    }
    server.sin_port = htons(data_port);
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        errorReport("Create socket error!");
    }
    if (connect(data_sock, (struct sockaddr*)&server, sizeof(server)) < 0)
        errorReport("Cannot connect to server!");
    printf("Data connection successfully: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    sendCommand(sockfd, "STOR ", filename);
    if (getReplyCode(sockfd) == NO_SUCH_FILE) {
        close(data_sock);
        fclose(src_file);
        return;
    }
    while ((bytes = fread(buf, 1, BUF_SIZE, src_file)) > 0)
        send(data_sock, buf, bytes, 0);

    close(data_sock);
    getReplyCode(sockfd);
    fclose(src_file);
}

void cmd_pwd(int sockfd) {
    sendCommand(sockfd, "PWD", "");
    if (getReplyCode(sockfd) != PATHNAME_CREATE)
        errorReport("Wrong reply for PWD!");
}

void cmd_dir(int sockfd) {
    int data_sock, bytes;
    char buf[BUF_SIZE] = {0};
    sendCommand(sockfd, "PASV", "");
    if (getReplyCode(sockfd) != PASV_MODE) {
        printf("Error!");
        return;
    }
    server.sin_port = htons(data_port);
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        errorReport("Create socket error!");
    if (connect(data_sock, (struct sockaddr*)&server, sizeof(server)) < 0)
        errorReport("Cannot connect to server!");
    printf("Data connection successfully: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    sendCommand(sockfd, "LIST ", "-al");
    getReplyCode(sockfd);
    printf("\n");

    while ((bytes = read(data_sock, buf, BUF_SIZE - 2)) > 0) {
        buf[bytes] = '\0';
        printf("%s", buf);
    }
    printf("\n");
    close(data_sock);
    getReplyCode(sockfd);
}

void cmd_cd(int sockfd, char* cmd) {
    int i = 0;
    char buf[BUF_SIZE];
    while (i < strlen(cmd) && cmd[i] != ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    while (i < strlen(cmd) && cmd[i] == ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    strncpy(buf, cmd+i, strlen(cmd+i)+1);
    sendCommand(sockfd, "CWD ", buf);
    getReplyCode(sockfd);
}

void cmd_help() {
    printf(" fget \t get a file from server.\n");
    printf(" fput \t send a file to server.\n");
    printf(" servpwd \t get the present directory on server.\n");
    printf(" clipwd \t get the present directory on client.\n");
    printf(" servls \t list the directory on server.\n");
    printf(" clils \t list the directory on client.\n");
    printf(" servcd \t change the directory on server.\n");
    printf(" ?/help\t help you know how to use the command.\n");
    printf(" quit \t quit client.\n");
}

void cmd_quit(int sockfd) {
    sendCommand(sockfd, "QUIT", "");
    if (getReplyCode(sockfd) == CONTROL_CLOSE)
        printf("Logout.\n");
}

void getclicwd()
{
	char buf[10240];
	int temp, n;
    system("ls -l>.temp");
    temp = open("./.temp", O_RDONLY);
    if (temp < 0)
        puts ("OPEN .temp ERROR");
    else
        n = read(temp,buf,10240);
    printf("%s",buf);
    system("rm -f ./.temp");
}

void run(char* ip, char* pt) {
    int  sockfd = connectToHost(ip, pt);
    if (getReplyCode(sockfd) != SERVICE_READY)
        errorReport("Service Connect Error!");
    int isQuit = 0;
    char buf[BUF_SIZE];
    char cwd[1024];
    while (!isQuit) {
        printf("myftp > ");
        fgets(buf, sizeof(buf), stdin);
        switch (cmdToNum(buf)) {
            case GET:
                cmd_get(sockfd, buf);
                break;
            case PUT:
                cmd_put(sockfd, buf);
                break;
            case PWD:
                cmd_pwd(sockfd);
                break;
            case DIR:
                cmd_dir(sockfd);
                break;
            case CD:
                cmd_cd(sockfd, buf);
                break;
            case HELP:
                cmd_help();
                break;
            case QUIT:
                cmd_quit(sockfd);
                isQuit = 1;
                break;
            case CPWD:
                getcwd(cwd, sizeof(cwd));
    			printf("Current working dir: %s\n", cwd);
                break;
            case CLS:
            	getclicwd();
                break;
            case CLEAR:
            	system("clear");
            	break;
            default:
            	printf("Invalid Command\n");
                cmd_help();
                break;
        }
    }
    close(sockfd);
}

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 3) {
        printf("Usage: %s <host> [<port> Default is 21]\n", argv[0]);
        exit(-1);
    }
    else if (argc == 2)
        run(argv[1], "21");
    else
        run(argv[1], argv[2]);
}
