#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <regex.h>

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

using namespace std;

static void *sendSignal(void*);

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

	int r;
	regex_t reg;

    if (r = regcomp(&reg, "^(\\d{0,3}\\.){3}\\d{0,3}$", REG_NOSUB | REG_EXTENDED) ){
        char errbuf[1024];
        regerror(r, &reg, errbuf, sizeof(errbuf));
        printf("error: %s\n", errbuf);
        return 1;
    }
    
    if (regexec(&reg, argv[1],0,NULL,0) != REG_NOMATCH){
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
    cout<<serv_addr.sin_port<<endl;

    /* Create new thread */
  	pthread_t send_signal_thread;

    // "CONSUME" THREAD
  	if(pthread_create(&send_signal_thread, NULL, &sendSignal, NULL)){
  		fprintf(stderr, "Error creating thread\n");
  		return 1;
  	}

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
      bool xx = false;
        while (lastSignalRecv == XOFF) {
          if (!xx){
              printf("Menunggu XON...\n");
              xx = true;
          }
        }

        printf("Mengirim byte ke-%d: '%s'\n", counter, buffer);
        sendto(sockfd,buffer, strlen(buffer),0,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
        bzero(buffer, MAXLEN);
        counter++;
    }

    printf("Exiting parent...\n");
   return 0;
}

// Thread function
static void *sendSignal(void* param){
  printf("Hello I am the child process\n");
  printf("My pid is %d\n",getpid());

  while (true) {

      int serv_len = sizeof(serv_addr);

      char _buffer[MAXLEN];

      n = recvfrom(sockfd,_buffer,strlen(_buffer),0,(struct sockaddr *)&serv_addr,(socklen_t*) &serv_len);
      // buffer[n]=0;
      // fputs(_buffer,stdout);

      if (n < 0) {
         perror("ERROR reading from socket");
         exit(1);
      }

      lastSignalRecv = _buffer[0];
      if (lastSignalRecv == XOFF) {
          printf("XOFF diterima.\n");
      } else if (lastSignalRecv == XON) {
          printf("XON diterima.\n");
      }

  }
  printf("Exiting child...\n");
	pthread_exit(0);
}
