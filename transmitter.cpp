#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <regex>

/* Maximum messages length */
#define MAXLEN 2

#define XON (0x11)
#define XOFF (0x13)

int sockfd, port, n;
struct sockaddr_in serv_addr;
struct hostent *server;
FILE *fp;
bool is_ip_address;
char buffer[MAXLEN], lastSignalRecv = XON;

int main(int argc, char *argv[]){

    printf("Memulai program ...");

    if (argc < 4) {
        fprintf(stderr,"Usage : %s hostname port filename\n", argv[0]);
        exit(0);
    }

    port = atoi(argv[2]);

    /* Create a socket point */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    /* Empty buffer */
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;

    if (std::regex_match (argv[1], std::regex("^(\\d{0,3}\\.){3}\\d{0,3}$") )){
        is_ip_address = true;
    }

    if ( is_ip_address ) {
        serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

        printf("Membuat socket untuk koneksi ke %s:%s ...", argv[1], argv[2]);
    } else {
        /* Map host name to ip address */
        server = gethostbyname(argv[1]);

        if (!server) {
           fprintf(stderr,"ERROR, no such host\n");
           exit(0);
        }

        bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    }

    /* converts a port number in host byte order to a port number in network byte order */
    serv_addr.sin_port = htons(port);

    pid_t pid = fork();

    if ( pid == 0 ) { // Child Process
        printf("Hello I am the child process\n");
        printf("My pid is %d\n",getpid());

        while (true) {
            n = recvfrom(sockfd,buffer,MAXLEN,0,NULL,NULL);
            // buffer[n]=0;
            fputs(buffer,stdout);

            if (n < 0) {
               perror("ERROR reading from socket");
               exit(1);
            }

            lastSignalRecv = buffer[0];
            if (lastSignalRecv == XOFF) {
                printf("XOFF diterima.");
            } else if (lastSignalRecv == XON) {
                printf("XON diterima.");
            }
        }
    } else if (pid > 0 ) { // Parent Process
        printf("Hello I am the parent process \n");
        printf("My actual pid is %d \n",getpid());
        printf("Opening file %s \n",argv[3]);

        /* opening file for reading */
        fp = fopen(argv[3] , "r");
        if(fp == NULL)
        {
           perror("Error opening file");
           exit(1);
        }

        int counter = 1;

        while(fgets(buffer, MAXLEN, fp) != NULL ) {
            while (lastSignalRecv == XOFF) {
                printf("Menunggu XON...");
            }

            printf("Mengirim byte ke-%d: '%s'\n", counter, buffer);
            sendto(sockfd,buffer, strlen(buffer),0,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
            bzero(buffer, MAXLEN);

            counter++;
        }
    } else {
        printf("Error\n");
        exit(1);
    }

    printf("Exiting ...");

   return 0;
}
