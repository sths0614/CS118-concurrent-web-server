/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <unistd.h>
#include <string.h> //for strstr
#include <sys/stat.h> //for struct stat buf

// #define MAX_BUF_SIZE 1944 //completely arbitrary

// typedef struct httpRequest{ //parse the request string and then fill one of these types of structs
//      enum httpRequestMethod
//      {
//           GET = 0,
//           POST = 1
//      } method;
//      char* headers[47];
//      char* body;
// } httpRequest;

// typedef struct httpResponse{
//      int code; //404, etc
//      char* headers[47];
//      char* body;
// } httpResponse;

// int dynamicRead(const int file, void** buf) //file read with dynamic memory allocation
// {
//      int prevSize = 0;
//      int size = 256;
//      int bytesRead = 0;
//      *buf = calloc(size+1,1);
//      bytesRead = read(file, *buf, size-prevSize);
//      while (bytesRead == size-prevSize && size <= MAX_BUF_SIZE)
//      {
//           prevSize = size;
//           size *= 1.5;
//           *buf = realloc(*buf, size+1);
//           bytesRead = read(file, *buf, size-prevSize);
//      }
//      ((char*)(*buf))[size] = 0;
//      return prevSize + bytesRead;
// }

// int nextLineBreak(char* buf, char** end_R, char** nextline_R) //detect any combination of CRLF. *end_R will point to the beginning of CRLF sequence. *nextline_R will point to beginning of next line.
// {
//      int result = 0;
//      int len = 0;
//      char* nextline;
//      char* end = buf;
//      while (*end != '\n' && *end != '\r' && *end != 0 && len < MAX_BUF_SIZE)
//      {
//           len++;
//           end++;
//           if (*end == 0)
//           {
//                result = 1;
//           }
//      }
//      nextline = end;
//      while (*nextline == '\n' && *nextline == '\r' && *nextline != 0 && len < MAX_BUF_SIZE)
//      {
//           len++;
//           nextline++;
//           if (*nextline == 0)
//           {
//                result = 2;
//           }
//      }
//      *end_R = end;
//      *nextline_R = nextline;
//      return result;
// }

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void dostuff(int); /* function prototype */

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
     clilen = sizeof(cli_addr);
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/
     
     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
         if (newsockfd < 0) 
             error("ERROR on accept");
         
         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");
         
         if (pid == 0)  { // fork() returns a value of 0 to the child process
             close(sockfd);
             dostuff(newsockfd);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this 
     } /* end of while */
     return 0; /* we never get here */
}

int file_exist (char *filename)
{
  struct stat buffer;   
  return (stat (filename, &buffer) == 0);
}


/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void dostuff (int sock)
{
  int n;
  char temp[256];
  char buffer[256];
  char fname[256];
  char fent[256];
  char type[256];
  bzero(buffer,256);
  bzero(fname, 256);

  n = read(sock,buffer,255);
  if (n < 0) error("ERROR reading from socket");
  printf("Here is the message: %s\n",buffer);

  buffer[255] = 0;
  
  /*
  if (n == 256)
  {
       write(sock, "HTTP/1.1 413 Payload Too Large\r\n", 34);
       error("Request too long");
       return;
  }
  */

  // Check request type
  char* fpath_start = strstr(buffer, "GET /");
  if (fpath_start != buffer){
    write(sock, "HTTP/1.1 400 Bad Request", 25);
    error("ERROR Bad Request");
    return;
  }

  // Obtain file path
  fpath_start += 5; // "GET /"
  char* fpath_end = strstr(buffer, " HTTP");
  int fpath_length = fpath_end - fpath_start;
  strncpy(fname, fpath_start, fpath_length);
  fname[fpath_length] = '\0';
  printf("Filename is %s\n", fname);

  // Detect file extension
  char* fent_start = &fname[fpath_length];
  while (fent_start >= fname)
  {
       if (*fent_start == '.')
            break;
       else if (*fent_start == '/')
       {
            fent_start = 0;
            break;
       }
       fent_start--;
  }
  char* fent_end = &fname[fpath_length];
  int fent_length = fent_end - fent_start;
  
  // No file extension?
  if (fent_start < fname)
  {
       printf("No file extension");
       fent_length = 0;
       fent[0] = '\0';
  }
  else
  {
       strncpy(fent, fent_start, fent_length);
       fent[fent_length] = '\0';
       printf("File extension is %s\n", fent);
  }
     
  if (!file_exist(fname))
  {
    // printf ("404: Not Found\n");
    write(sock, "HTTP/1.1 404 Not Found\r\n", 24);
    write(sock, "Content-Language: en-US\r\n", 25);
    write(sock, "Content-Length: 0\r\n", 20);
    write(sock, "Content-Type: text/html\r\n", 25);
    write(sock, "Connection: close\r\n", 19);
    return;
  }

  // printf("Passed file existence test\n");

  // open file, get filesize
  FILE* f = fopen(fname, "rb");
  fseek(f, 0L, SEEK_END);
  int fsize = (int) ftell(f);
  fseek(f, 0L, SEEK_SET);
  printf("Filesize is %d\n", fsize);

  // read the file into fbuf
  char* fbuf = (char *)malloc(fsize*sizeof(char));
  fread(fbuf, 1, fsize, f);
  write(sock, "HTTP/1.1 200 OK\r\n", 17);
  write(sock, "Content-Language: en-US\r\n", 25);
  char formatted_CL[256];
  sprintf(formatted_CL, "Content-Length: %d\r\n", fsize);
  write(sock, formatted_CL, strlen(formatted_CL));

  // change content-type according to file extension
  if(strcmp(fent, ".jpg") == 0 || strcmp(fent, ".jpeg") == 0){
    strncpy(type, "image/jpeg\0", 11);
  }
  else if (strcmp(fent, ".gif") == 0){
    strncpy(type, "image/gif\0", 10);
  }
  else if (strcmp(fent, ".html") == 0){
    strncpy(type, "text/html\0", 10);
  }
  else{
    strncpy(type, "text/plain\0", 11);
  }
  printf("File type is %s\n", type);
  char formatted_CT[256];
  sprintf(formatted_CT, "Content-Type: %s\r\n", type);
  write(sock, formatted_CT, strlen(formatted_CT));
  write(sock, "Connection: close\r\n\r\n", 21);

  write(sock, fbuf, fsize);

  
  free(fbuf);
  fclose(f);
  return;
}
