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
char* print_ip(unsigned int ip);

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

	unsigned int A;
	unsigned int B;
	char a=0x00;
	char b=0x01;

	A=a<<8|b;
	B=b<<8|a;
	// 2printf("%c,%c,%d,%d\n",a,b,A,B);
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


		unsigned char request[99999];
		bzero(request,99999);
		if((recv(new_fd,request,99999,0))==-1){
			perror("recv error");
			return 1;
		}
		

        if(!fork()){  // child process
			// close(sockfd);
			unsigned char reply[8];
			reply[0]=0;
			reply[1]=0x5A;
			reply[2]=request[2];
			reply[3]=request[3];
			reply[4]=request[4];
			reply[5]=request[5];
			reply[6]=request[6];
			reply[7]=request[7];
            unsigned char VN = request[0];
            unsigned char CD = request[1];
			unsigned int DST_PORT;
        	unsigned int DST_IP;
			
            DST_PORT = (request[2] << 8 | request[3]);
            DST_IP = request[4] << 24 | request[5] << 16 | request[6] << 8 | request[7];
            char * USER_ID =request +8;

            printf("%c,%c,%u,%u,%s\n",VN,CD,DST_PORT,DST_IP,USER_ID);
			int32_t ip;
			ip=(int32_t)DST_IP;
			struct in_addr ip_addr;
			ip_addr.s_addr = ip;
			// printf("The IP address is %s\n", inet_ntoa(ip_addr));
			
			char * ip_chr=print_ip(DST_IP);
			printf("ip is :%s\n",ip_chr);
            if(VN==0x04 && CD==0x01) {
                printf("it is SOCKS request!\n");
				write(new_fd,reply,8);
				bzero(request,99999);
				recv(new_fd,request,99999,0);
				printf("-http request: %s\n\n\n",request);

				//connect to the client. just connect the distination.
				
				char http_reply[99999];
				bzero(http_reply,99999);
				//creat a socket
				int toDST = 0;
				toDST = socket(AF_INET , SOCK_STREAM , 0);

				if (toDST == -1){
					printf("Fail to create a socket.");
				}

				//socket的連線
				struct sockaddr_in info;
				bzero(&info,sizeof(info));
				info.sin_family = PF_INET;
				// info.sin_addr.s_addr = ip;
				// info.sin_port = htons(DST_PORT);
				// info.sin_addr.s_addr = inet_addr("140.113.207.238");
				info.sin_addr.s_addr = inet_addr(ip_chr);
				info.sin_port = htons(DST_PORT);
				
				int err = connect(toDST,(struct sockaddr *)&info,sizeof(info));
				if(err==-1){
					printf("Connection error");
				}else printf("\n\ntoDST:%d\n\n",toDST);
				write(toDST,request,(int)strlen(request));
				recv(toDST,http_reply,99999,0);
				write(new_fd,http_reply,(int)strlen(http_reply));


				/*int fd;
				int bytes_read;
				char data_to_send[1024];
				char *path="/home/ekko/index.html";
				if ( (fd=open(path, O_RDONLY))!=-1 )    //FILE FOUND
				{
					write(new_fd, "HTTP/1.1 200 OK\n\n", 17);
					// send(new_fd, "HTTP/1.0 200 OK\n\n", 17, 0);
					while ( (bytes_read=read(fd, data_to_send, BYTES))>0 )
						write (new_fd, data_to_send, bytes_read);
				}
				else    write(new_fd, "HTTP/1.1 404 Not Found\n\n", 23); //FILE NOT FOUND
				close(fd); */
            }else printf("it is not SOCKS request or Reject!\n");
			close(new_fd);
        }close(new_fd);  // parent doesn't need this.
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

char* print_ip(unsigned int ip)
{	
	char *ret;
	char ip_char[20];
	ret=ip_char;
    unsigned char bytes[4];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;
	sprintf(ip_char,"%d.%d.%d.%d",bytes[3], bytes[2], bytes[1], bytes[0]);
    printf("%d.%d.%d.%d\n", bytes[3], bytes[2], bytes[1], bytes[0]);
	// printf("%s\n",ip_char);
	return ret;
}