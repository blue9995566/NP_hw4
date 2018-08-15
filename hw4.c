#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define F_CONNECTING 0
#define F_READING 1
#define F_WRITING 2
#define F_DONE 3


//struct definition.
struct clientdata{
	int fd;
	char ip[200];
	int port;
    char sock_ip[200];
    int sock_port;
	char filename[50];
    FILE *fp;
    int status;
};

//global variable declaration
static struct clientdata clients[5];
static fd_set read_fds,write_fds,tmpread_fds,tmpwrite_fds;
static int max_fd;

//function forward declaration
static void querystringparse();
int create_connect(int index);
int fdtoindex(int fd);
int allDONE();

int recv_msg(int from);
int readline(int fd, char *ptr, int maxlen);
void printHtml(int index,char *msg,int cmd);
int main(void)
{
    // FILE *fp;
    char msg_buf[30000];
    int len;

    printf("Content-type: text/html\n\n");
    printf("<meta charset=\"UTF-8\">");
    printf("<title>Network Programming Homework 3</title>");


    querystringparse();

    printf("<body bgcolor=#336699>");
    printf("<font face=\"Courier New\" size=2 color=#FFFF99>");
    printf("<table width=\"800\" border=\"1\">");
    printf("<tr>");
    for(int i=0;i<5;i++)printf("<td>%s</td>",clients[i].ip);
    printf("</tr>");
    printf("<tr>");
    for(int i=0;i<5;i++)printf("<td valign=\"top\" id=\"m%d\"></td>",i);
    printf("</tr></table>");

    //initialize fdset.
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&tmpread_fds);
    FD_ZERO(&tmpwrite_fds);
    char buf[256];    // buffer for client data
    int nbytes;

    for(int i=0;i<5;i++){
        if(create_connect(i)<0) clients[i].status = F_DONE; //something wrong!
        // printf("<br>client%d's fd,status : %d,%d\n",i+1,clients[i].fd,clients[i].status);
        // printf("<br>client%d's ip : %s\n",i+1,clients[i].ip);
        // printf("<br>client%d's port : %d\n",i+1,clients[i].port);
        // printf("<br>client%d's filename : %s\n",i+1,clients[i].filename);
        // printf("<br>client%d's sock_ip : %s\n",i+1,clients[i].sock_ip);
        // printf("<br>client%d's sock_port : %d\n",i+1,clients[i].sock_port);

    }

    int a=0;    
    while(allDONE()){
        for(int i=0;i<5;i++){
            // printf("%d-Status:%d<br>",i+1,clients[i].status); 
        }
        a++;
        // copy it.
        tmpread_fds = read_fds;
        tmpwrite_fds = write_fds;
        int ret_select=select(max_fd+1, &tmpread_fds, &tmpwrite_fds, NULL, NULL);
        // printf("(%d,ret_select%d)<br>",a,ret_select);
        switch (ret_select){
            case -1:
                perror("select error");
                break;
            case 0:
                perror("select time out");
                break;
            default:
                break;
        }
        // printf("--%d",max_fd);
        // if(a%50==0)sleep(3);

        for(int i = 0; i <= max_fd; i++) {
            if(clients[fdtoindex(i)].status==F_READING && FD_ISSET(clients[fdtoindex(i)].fd, &tmpread_fds)){
                int errnum;
                errnum = recv_msg(clients[fdtoindex(i)].fd); // print result.

                if(errnum <=0) clients[fdtoindex(i)].status = F_DONE;
                /*if (errnum < 0) // error.
                {
                    shutdown(client_fd, 2);
                    close(client_fd);
                    exit(1);
                }
                else if (errnum == 0)  // clent close.
                {
                    shutdown(client_fd, 2);
                    close(client_fd);
                    exit(0);
                }*/

                // FD_CLR(i, &read_fds);
                // FD_SET(i, &write_fds);
                // clients[fdtoindex(i)].status = F_WRITING;
            }else if(clients[fdtoindex(i)].status==F_WRITING && FD_ISSET(clients[fdtoindex(i)].fd, &tmpwrite_fds)){
                len = readline(fileno(clients[fdtoindex(i)].fp), msg_buf, sizeof(msg_buf));
                if (len < 0) exit(1);
                if(len==0){
                    clients[fdtoindex(i)].status = F_DONE;
                }else{
                msg_buf[len-1] = 13;  // /r
                msg_buf[len] = 10;		// /n
                msg_buf[len+1] = '\0';
                printHtml(fdtoindex(i),msg_buf,1);
                // printf("%s", msg_buf);
                fflush(stdout);
                if (write(clients[fdtoindex(i)].fd, msg_buf, len + 1) == -1)
                    return -1;
                // gSc = 0; // set it to print the result from server.
                FD_CLR(i, &write_fds);
                FD_SET(i, &read_fds);
                clients[fdtoindex(i)].status = F_READING;
                }
            }else if(clients[fdtoindex(i)].status==F_DONE){
                FD_CLR(i, &read_fds);
                FD_CLR(i, &write_fds);
                close(i);
            }else if(clients[fdtoindex(i)].status==F_CONNECTING){
                clients[fdtoindex(i)].status=F_READING;
            }
        }
        /*printf("%d,allDONE():%d<br>",a,allDONE());
        for(int i=0;i<5;i++){
            printf("%d-Status:%d<br>",i+1,clients[i].status); 
        }
        fflush(stdout);*/
    }
    printf("</body>");

    return 0;
}

static void querystringparse(){ //it doesn't need any argument, query_string can get from env vars.
    char *querystring=getenv("QUERY_STRING");
    char *value;
    char *tok;
    int index;
    if (querystring!=NULL){
        // printf("%s\n",querystring);
        tok= strtok(querystring,"&");
        while(tok!=NULL){
            value=tok+3;
            if(tok[0]=='h'){
                index=(int)tok[1]-48-1;
                strcpy(clients[index].ip,value);
            }else if (tok[0]=='p'){
                char num[5];
                for (int i=0;i<5;i++){  // initialize
                    num[i]=' ';
                }
                int i=0;
                while(isdigit(*value)){
                    i++;
                    value++;
                }
                if(i>0){
                    strncpy(num,value-i,i);
                    i=atoi(num);
                }
                clients[index].port=i;
            }else if(tok[0]=='s'&& tok[1]=='h'){
                value++;
                strcpy(clients[index].sock_ip,value);
            }else if (tok[0]=='s'&& tok[1]=='p'){
                value++;
                char num[5];
                for (int i=0;i<5;i++){  // initialize
                    num[i]=' ';
                }
                int i=0;
                while(isdigit(*value)){
                    i++;
                    value++;
                }
                if(i>0){
                    strncpy(num,value-i,i);
                    i=atoi(num);
                }
                clients[index].sock_port=i;
            }else strcpy(clients[index].filename,value);

            tok=strtok(NULL,"&");

        }
    }
}



int create_connect(int index){
    struct hostent *he;
    if ((he = gethostbyname(clients[index].sock_ip)) == NULL)
	{
		fprintf(stderr, "wrong hostname or ip\n");
		return -1;
	}
    clients[index].fp = fopen(clients[index].filename, "r"); // filename
    if (clients[index].fp == NULL)
    {
        fprintf(stderr, "Error : '%s' doesn't exist\n",clients[index].filename);
        return -1;
    }

	struct sockaddr_in client_sin;
    
    clients[index].fd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&client_sin, sizeof(client_sin));
	client_sin.sin_family = AF_INET;
	client_sin.sin_addr = *((struct in_addr *)he->h_addr);     
	client_sin.sin_port = htons((u_short)clients[index].sock_port);
    
    fcntl(clients[index].fd, F_SETFL, O_NONBLOCK); //set the socket nonblocking.

    int flag = fcntl(clients[index].fd, F_GETFL, 0);
    fcntl(clients[index].fd, F_SETFL, flag | O_NONBLOCK);
    int ret=connect(clients[index].fd, (struct sockaddr *)&client_sin, sizeof(client_sin));
    if (ret == 0) {
        clients[index].status=F_READING;  //success , acctually this fd set write.
    }else{
        if (errno == EINPROGRESS) {
            clients[index].status=F_CONNECTING;  //connecting 
        }else clients[index].status=F_DONE; //error
    }

    unsigned int ip3,ip2,ip1,ip0;
    sscanf(clients[index].ip,"%d.%d.%d.%d",&ip3,&ip2,&ip1,&ip0);
    unsigned char sock_request[8];
    bzero(sock_request,8);
    sock_request[0]=4;
    sock_request[1]=1;
    sock_request[2]=clients[index].port/256;
    sock_request[3]=clients[index].port%256;
    sock_request[4]=ip3;
    sock_request[5]=ip2;
    sock_request[6]=ip1;
    sock_request[7]=ip0;


    int w=write(clients[index].fd,sock_request,8);
    // printf("clients[index].fd:%d,write:%d\n",clients[index].fd,w);
    unsigned char sock_reply[8];
    bzero(sock_reply,8);
    int n;
    while(1){
        n=read(clients[index].fd,sock_reply,8);
        if(n>=0)break;
    }
    // printf("recv:%d,%d,%s\n",n,sock_reply[1],sock_reply);

    fflush(stdout);
    
    if(sock_reply[1]==0x5A){
        clients[index].status=F_READING;
    }else{
        clients[index].status=F_DONE; //error
        return -1;
    }
    // printf("<br><b>connect successfully!fd:%d</b>",clients[index].fd);
    FD_SET(clients[index].fd, &read_fds);
    FD_SET(clients[index].fd, &write_fds);
    if(clients[index].fd>max_fd) max_fd=clients[index].fd;
    return 1;
}



int fdtoindex(int fd){
    for (int i =0 ; i<5 ; i++){
        if (clients[i].fd==fd){
            return i;
        }
    }
    return -1;
}

int allDONE(){
    for(int i=0;i<5;i++){
        if(clients[i].status!=F_DONE) return 1;
    }
    return 0;
}

int recv_msg(int from)
{
	char buf[3000];
	int len;
	if ((len = readline(from, buf, sizeof(buf) - 1)) < 0)
		return -1;
	buf[len] = 0;
    printHtml(fdtoindex(from),buf,0);
	// printf("%s", buf); //echo input
	fflush(stdout);
	return len;
}

int readline(int fd, char *ptr, int maxlen)
{
	int n, rc;
	char c;
	*ptr = 0;
	for (n = 1; n < maxlen; n++)
	{
		if ((rc = read(fd, &c, 1)) == 1)  //read one character.
		{
			*ptr++ = c;
			if (c == ' ' && *(ptr - 2) == '%')
			{
                FD_CLR(fd, &read_fds);
                FD_SET(fd, &write_fds);
                clients[fdtoindex(fd)].status=F_WRITING;
				// gSc = 1; // need to write.
				break;
			}
			if (c == '\n')
				break;
		}
		else if (rc == 0)
		{
			if (n == 1)
				return (0);
			else
				break;
		}
		else
			return (-1);
	}
	return (n);
}

void printHtml(int index,char *msg,int cmd)
{
    printf("<script>document.all['m%d'].innerHTML +=\"",index);
    int i = 0;
    if(cmd) printf("<b>");
    while (msg[i] != '\0')
    {
        switch(msg[i])
        {
            case '<':
                printf("&lt");
                break;
            case '>':
                printf("&gt");
                break;
            case ' ':
                printf("&nbsp;");
                break;
            case '\r':
                if (msg[i+1] == '\n')
                {
                    printf("<br>");
                    i++;
                }
                break;
            case '\n':
                printf("<br>");
                break;
            case '\\':
                printf("&#039");
                break;
            case '\"':
                printf("&quot;");
                break;
            default:
                printf("%c", msg[i]);
        }
        i++;
    }
    if(cmd) printf("</b>");
    printf("\";</script>\n");
}