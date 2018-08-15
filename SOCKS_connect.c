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
#include <time.h>

struct accept_entry{
	int ip3;
	int ip2;
	int ip1;
	int ip0;
	char mode;	
};

// variable define
#define PORT "6789"  // the port users will be connecting to
#define BACKLOG 10	 // how many pending connections queue will hold
#define BYTE 1024

// function forward declaration
void parse_http_request(char * request);
void print_ip(unsigned int ip);
int check_permit(char mode,char* ip);

//global variable declaration
static char *filename;
static char ip_char[20];
static struct accept_entry accept_entrys[100];
static int entry_num=0;

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
// get port, IPv4 or IPv6:
in_port_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}


int main(void)
{
	printf("int:%d,char:%d\n",(int)sizeof(int),(int)sizeof(char));
	FILE * fp;
	fp = fopen ("socks.conf", "r");
	if(fp!=NULL){
		char string[128];
		while(fgets(string,128,fp)!=NULL){
			sscanf(string,"permit %c %d.%d.%d.%d",&accept_entrys[entry_num].mode,&accept_entrys[entry_num].ip3,&accept_entrys[entry_num].ip2,&accept_entrys[entry_num].ip1,&accept_entrys[entry_num].ip0);
			// printf("mode:%c %d.%d.%d.%d\n",accept_entrys[entry_num].mode,accept_entrys[entry_num].ip3,accept_entrys[entry_num].ip2,accept_entrys[entry_num].ip1,accept_entrys[entry_num].ip0);
			if(accept_entrys[entry_num].mode!='b'&& accept_entrys[entry_num].mode!='c') entry_num--;
			entry_num++;
		}
	}
	fclose(fp);

	printf("entry_num:%d\n",entry_num);
	for(int i =0; i< entry_num;i++){
		printf("mode:%c %d.%d.%d.%d\n",accept_entrys[i].mode,accept_entrys[i].ip3,accept_entrys[i].ip2,accept_entrys[i].ip1,accept_entrys[i].ip0);
	}
	
	
	
	
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
		printf("new_fd,sockfd:%d,%d\n",new_fd,sockfd);
		if (new_fd == -1) {
			perror("accept error");
			continue;
		}
		inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);
		unsigned int client_port=((struct sockaddr_in *)&their_addr)->sin_port;
		printf("server: got connection from %s\n", s);


		unsigned char request[99999];
		bzero(request,99999);
		if((recv(new_fd,request,99999,0))==-1){
			perror("recv error");
			return 1;
		}
		
		int pid=fork();
        if(pid==0){  // child process
			close(sockfd);
			fd_set read_fds,tmpread_fds;
			FD_ZERO(&read_fds);
			FD_ZERO(&tmpread_fds);
			FD_SET(new_fd,&read_fds);

            unsigned char VN = request[0];
            unsigned char CD = request[1];
			unsigned int DST_PORT;
        	unsigned int DST_IP;
			
            DST_PORT = (request[2] << 8 | request[3]);
            DST_IP = request[4] << 24 | request[5] << 16 | request[6] << 8 | request[7];
            char * USER_ID =request +8;

            // printf(",%c,%c,%u,%u,%s\n",VN,CD,DST_PORT,DST_IP,USER_ID);
			
			int32_t ip;
			ip=(int32_t)DST_IP;
			struct in_addr ip_addr;
			ip_addr.s_addr = ip;
			// printf("The IP address is %s\n", inet_ntoa(ip_addr));
			
			// unsigned char * ip_chr=malloc(20 * sizeof(char));
			// ip_chr=print_ip(DST_IP);
			print_ip(DST_IP);
			// printf("dis-ip is :%s\n",ip_char);
			
			printf("<S_IP>:%s\n",s);
			printf("<S_PORT>:%d\n",client_port);
			printf("<D_IP>:%s\n",ip_char);
			printf("<D_PORT>:%d\n",DST_PORT);
			char *mode = (CD==0x01)?"CONNECT":"BIND";
			printf("<Command>:%s\n",mode);
			int permit=check_permit((CD==0x01)?'c':'b',ip_char);
			char *sock_replay= (permit)?"ACCEPT":"REJECT";
			printf("<Reply>:%s\n",sock_replay);
			printf("<Content>:\n");

			
            if(VN==0x04 && CD==0x01) {
				unsigned char reply[8];
				reply[0]=0;
				reply[1]=0x5A;
				reply[2]=request[2];
				reply[3]=request[3];
				reply[4]=request[4];
				reply[5]=request[5];
				reply[6]=request[6];
				reply[7]=request[7];
                printf("it is SOCKS request!\n");
				if(!permit){
					reply[1]=0x5B;
					write(new_fd,reply,8);
					exit(0);
				}else {
					int w=write(new_fd,reply,8);
					printf("write:%d\n",w);
				}
				
				bzero(request,99999);
				// recv(new_fd,request,99999,0);
				// if(strstr(request,"GET")!=NULL) printf("it is http request!!\n");
				// printf("request: %s\n\n\n",request);

				//connect to the client. just connect the distination.
				char DST_reply[99999];
				bzero(DST_reply,99999);
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
				printf("*ip is :%s\n",ip_char);
				info.sin_addr.s_addr = inet_addr(ip_char);
				info.sin_port = htons(DST_PORT);
				
				int err = connect(toDST,(struct sockaddr *)&info,sizeof(info));
				if(err==-1){
					printf("Connection error");
				}else printf("\n\ntoDST:%d\n\n",toDST);

				FD_SET(toDST,&read_fds);

				int flag=1;

				while(flag){
					tmpread_fds = read_fds;
					select(getdtablesize(), &tmpread_fds, NULL, NULL, NULL);
					if(FD_ISSET(new_fd,&tmpread_fds)){
						bzero(request,99999);  //unsign or sign
						int n=recv(new_fd,request,99999,0);
						if(n==0) break;
						// printf("request:%s\n",request);
						write(toDST,request,n);
												
					}else if(FD_ISSET(toDST,&tmpread_fds)){
						bzero(DST_reply,99999);
						// char DST_reply[1024]={0};
						int n =recv(toDST,DST_reply,99999,0);
						if(n==0) break;
						// char cpreply[99999];
						// strcpy(cpreply,DST_reply);
						// if(strstr(cpreply,"</html>")!=NULL) flag=0;
						// printf("DST_reply:%s\n",DST_reply);
						fflush(stdout);
						write(new_fd,DST_reply,n);
					}
				}close(toDST);
            }else if(VN==0x04 && CD==0x02) {
                printf("it is SOCKS BIND mode!\n");
				
				bzero(request,99999);
				srand(time(NULL));
				unsigned int port=12345+(rand()%1000);
				printf("--port:%d\n",port);
				unsigned char reply[8];
				reply[0]=0;
				reply[1]=0x5A;
				//port
				reply[2]=port/256;
				reply[3]=port%256;
				//ip
				reply[4]=0;
				reply[5]=0;
				reply[6]=0;
				reply[7]=0;

				if(!permit){
					reply[1]=0x5B;
					write(new_fd,reply,8);
					exit(0);
				}
			
				int bindsock= socket(AF_INET , SOCK_STREAM , 0);
				if (bindsock == -1){
					printf("Fail to create a socket.");
				}
				struct sockaddr_in bind_addr;
				bzero((char*) &bind_addr, sizeof(bind_addr));   
				bind_addr.sin_family = AF_INET;
				bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
				bind_addr.sin_port = htons(port);

				if (bind(bindsock, (struct sockaddr*) &bind_addr, sizeof(bind_addr)) < 0)
				{
					perror("bind error\n");
					return -1;
				}

				if (listen(bindsock, 5) < 0){
					perror("listen fail\n");
					exit(-1);
				}

				printf("bind OK!fd:%d",bindsock);

				write(new_fd,reply,8);

				char DST_reply[99999];
				bzero(DST_reply,99999);

				int toDST;
				struct sockaddr_storage dst_addr;
				sin_size = sizeof dst_addr;
				toDST = accept(bindsock, (struct sockaddr *)&dst_addr, &sin_size);
				if (toDST == -1){
					printf("Fail to accept a socket.");
				}

				FD_SET(toDST,&read_fds);

				int flag=1;
				while(flag){
					tmpread_fds = read_fds;
					select(getdtablesize(), &tmpread_fds, NULL, NULL, NULL);
					if(FD_ISSET(new_fd,&tmpread_fds)){
						// bzero(request,1024);  //unsign or sign
						unsigned char requests[1024]={0};
						// bzero(requests,1024);
						int n=recv(new_fd,requests,1024,0);
						if(n==0) break;
						printf("----(B)request:%s\n",requests);
						write(toDST,requests,n);
												
					}else if(FD_ISSET(toDST,&tmpread_fds)){
						// bzero(DST_reply,1024);
						unsigned char D_reply[1024]={0};
						// bzero(D_reply,1024);
						int n =recv(toDST,D_reply,1024,0);
						if(n==0) break;
						printf("----(B)DST_reply:%s\n",D_reply);
						fflush(stdout);
						write(new_fd,D_reply,n);
					}
				}close(toDST);
            }
			else{
				// printf("it is not SOCKS request or Reject!\n");
				;
			}
			close(new_fd);
			exit(0);
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

void print_ip(unsigned int ip)
{	
	// unsigned char *ret;
	// char ip_char[20];
	bzero(ip_char,20);
	// ret=ip_char;
    unsigned char bytes[4];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;
	sprintf(ip_char,"%d.%d.%d.%d",bytes[3], bytes[2], bytes[1], bytes[0]);
    printf("%d.%d.%d.%d\n", bytes[3], bytes[2], bytes[1], bytes[0]);
	// printf("%s\n",ip_char);
	// return ret;
}
int check_permit(char mode,char* ip){
	int ip3,ip2,ip1,ip0;
	sscanf(ip,"%d.%d.%d.%d",&ip3,&ip2,&ip1,&ip0);
	printf("%d %d %d %d\n",ip3,ip2,ip1,ip0);
	for(int i =0; i< entry_num;i++){
		if(accept_entrys[i].mode==mode){
			int tmp=(( !accept_entrys[i].ip3 || accept_entrys[i].ip3==ip3  )&&(  !accept_entrys[i].ip2 || accept_entrys[i].ip2==ip2 )&&( !accept_entrys[i].ip1 || accept_entrys[i].ip1==ip1  )&&(  !accept_entrys[i].ip0 || accept_entrys[i].ip0==ip0 ));
			if(tmp==1) return 1;
		}else continue;
		// printf("mode:%c %d.%d.%d.%d\n",accept_entrys[i].mode,accept_entrys[i].ip3,accept_entrys[i].ip2,accept_entrys[i].ip1,accept_entrys[i].ip0);
	}
	return 0;
}