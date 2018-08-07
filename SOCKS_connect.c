/*
** SOCKS_connect.c -- the np4 part one.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h> //file control

// variable define
#define PORT "6789"  // the port users will be connecting to
#define BACKLOG 10	 // how many pending connections queue will hold
#define BYTES 1024

// function forward declaration
void parse_http_request(char * request);

//global variable declaration
static char *filename;

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		//it prevnet the error message "Address already in use."
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}

	freeaddrinfo(servinfo);  // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
	printf("server: waiting for connections...\n");

    
	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}
		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		char request[99999];
		bzero(request,99999);
		if((recv(new_fd,request,99999,0))==-1){
			perror("recv error");
			return 1;
		}
        if(!fork()){  // child process
            unsigned char VN = request[0];
            unsigned char CD = request[1];
            unsigned int DST_PORT = request[2]<< 8 | request[3];  // 1151:-128 1279:-1
            unsigned int DST_IP = request[4] <<24 | request[5] <<16 | request[6] <<8 | request[7];
            char * USER_ID =request +8;

            printf("%c,%c,%u,%u,%s\n",VN,CD,DST_PORT,DST_IP,USER_ID);
            if(VN==0x04) {
                printf("it is SOCKS request!\n");
            }else printf("it is not SOCKS request!\n");
            if(CD==0x01) {
                printf("Accept!\n");
            }else printf("Reject!\n");
        }
		close(new_fd);  // parent doesn't need this.
	}

	return 0;
}


//Parse the request and set the "QUERY_STRING" environment variables.
void parse_http_request(char * request){
	char *tok;
	char *mark;
	char *query;
	tok=strtok(request," \t");
	while(strstr(tok,"HTTP")==NULL){
		// printf("%s\n",tok);
		if(strstr(tok,"?")!=NULL){
			mark=strchr(tok,'?');
			filename=strtok(tok,"?");
			query=mark+1;
			break;
		}
		filename=tok;
		query=NULL;
		tok=strtok(NULL," \t");
	}
	if(strncmp(filename,"/\0",2)==0) filename="/index.html";
	if(query!=NULL) setenv("QUERY_STRING",query,1);
	else unsetenv("QUERY_STRING");
}
