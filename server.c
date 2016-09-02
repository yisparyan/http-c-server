#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <dirent.h>

#include <string.h>
#include <stdlib.h>

#include <sys/stat.h> //open()
#include <sys/wait.h> //waitpid()
#include <fcntl.h>    //open()

#include <time.h>

#include "http_error_html.h" //Macros for http error html responses


#define PORT argv[1]
#define BACKLOG 20
#define SERVER_NAME "yri-server"
#define SERVER_VERSION "0.0.1"
#define SERVER_HTTP_VERSION "HTTP/1.0"

#define RESPONSE_BUFFER_SIZE 1000000 //1000kb
#define REQUEST_BUFFER_SIZE 8000 //8000b is recommended minimum by spec

#define MAX_ADDL_HEADERS 20 

#define CRLF "\r\n"

#define STRLEN(string) (sizeof(string)/sizeof(string[0]) - 1)

/*
 *   TODO:
 *   Currently, you can access above the current directory by sending a GET
 *   request with domain:port/../somefile.txt. This needs to be fixed, so you
 *   can't access above where the server was run.
 */

#define HTTP_METHOD_INVALID 0x00
#define HTTP_METHOD_GET     0x01
#define HTTP_METHOD_POST    0x02
#define HTTP_METHOD_HEAD    0x03
#define HTTP_METHOD_PUT     0x04
#define HTTP_METHOD_DELETE  0x05
#define HTTP_METHOD_TRACE   0x06
#define HTTP_METHOD_OPTIONS 0x07
#define HTTP_METHOD_CONNECT 0x08
#define HTTP_METHOD_PATCH   0x09

struct pair {
    char* name;
    char* value;
};

struct headers {
  struct pair* h;
  int size;
  const int max_length;
};

struct http_request {
    int method;
    char* uri;
    char* query;
    char* version;
    struct pair parameters[20];
    int parameters_length;
    char* msgbody;
};




int send_all(int sockfd, char* msg, int msg_size)
{
  printf("msg size: %d bytes\n", msg_size);
  int total_bytes_sent = 0;
  while(msg_size - total_bytes_sent > 0) {
    int bytes_sent = send(sockfd, &msg[total_bytes_sent], 
                          msg_size - total_bytes_sent, 0);
    if(bytes_sent == -1) {
      perror("send");
      return -1;
    }
    total_bytes_sent += bytes_sent;
    printf("Sent %d bytes! %d bytes left.\n", bytes_sent, 
            msg_size-total_bytes_sent);
  }
  printf("Finished sending data sucessfully.\n");
  return 0;
}
int get_time_string(char* out_ftime_buffer, size_t buffer_size)
{
  time_t rawtime;
  struct tm* ptm;
  time(&rawtime);
  ptm = gmtime(&rawtime);
  return strftime(out_ftime_buffer, buffer_size, "%a, %d %b %Y %X GMT", ptm);
}


int insert_header(struct headers* headers, char* name, char* value)
{
  if(headers->size < headers->max_length) {
    headers->h[headers->size].name = name;
    headers->h[headers->size].value = value;
    return ++headers->size;
  }
  return 0;
}

int add_http_header(char* buffer, char* header_name, char* header_value)
{
  strcat(buffer, header_name);
  strcat(buffer, ": ");
  strcat(buffer, header_value);
  strcat(buffer, CRLF);
  return strlen(header_name) + strlen(header_value) + 
          STRLEN(CRLF) + STRLEN(": ");
}

// Input buffer must already be set to 0 and this functions assumes
// the buffer large enough to store the http headers
int build_http_response_header(char* buffer, int buffer_size,
                               char* content_type, off_t content_length, 
                               int status_code, struct headers* headers)
{
  strcat(buffer, SERVER_HTTP_VERSION " ");
  char* status;
  switch(status_code) {
    case 200:
      status = "200 OK";
      break;
    case 301:
      status = "301 Moved";
      break;
    case 404:
      status = "404 Not Found";
      break;
    default:
      status = "500 Internal Server Error";
      break;
  }
  strcat(buffer, status);
  strcat(buffer, CRLF);

  if(content_type != 0){
    add_http_header(buffer, "Content-Type", content_type);
  }
  add_http_header(buffer, "Server", SERVER_NAME);

  strcat(buffer, "Date: ");
  int offset = strlen(buffer);
  get_time_string(&buffer[offset], buffer_size - offset);
  strcat(buffer, CRLF);

  if(content_length > 0) {
    strcat(buffer, "Content-Length: ");
    offset = strlen(buffer);
    snprintf(&buffer[offset], buffer_size - offset, 
             "%lld", (long long) content_length);
    strcat(buffer, CRLF);
  }
  
  //Add headers into buffer from addtional headers list
  int currentpos = strlen(buffer);
  if(headers != 0) {
    for(int i = 0; i < headers->size; i++) {
      currentpos += add_http_header(&buffer[currentpos], 
                                    headers->h[i].name, 
                                    headers->h[i].value);
    }
  }

  strcat(buffer, CRLF);
  return currentpos + STRLEN(CRLF);
}


int send_http_error_response(int sockfd, int status_code)
{
  char buffer[10000] = {0};
  int msg_size = 0;
  switch(status_code) {
    case 400:
      msg_size = build_http_response_header(buffer, 10000, "text/html", 
                                            STRLEN(HTTP_400_HTML), 
                                            status_code, 0);
      strcat(buffer, HTTP_400_HTML);
      msg_size += STRLEN(HTTP_400_HTML);
      break;

    case 404:
      msg_size = build_http_response_header(buffer, 10000, "text/html", 
                                            STRLEN(HTTP_404_HTML), 
                                            status_code, 0);
      strcat(buffer, HTTP_404_HTML);
      msg_size += STRLEN(HTTP_404_HTML);
      break;
    case 501: //TODO: add the html page for this
    case 500:
    default:
      msg_size = build_http_response_header(buffer, 10000, "text/html", 
                                            STRLEN(HTTP_500_HTML), 
                                            status_code, 0);
      strcat(buffer, HTTP_500_HTML);
      msg_size += STRLEN(HTTP_500_HTML);
      break;
  }
  
  return send_all(sockfd, buffer, msg_size);
}

int is_valid_http_version(char* version)
{
  if(strncmp(version, "HTTP/1.0", 8) == 0) {
    return 1;
  }
  else if(strncmp(version, "HTTP/1.1", 8) == 0) {
    return 1;
  }
  return 0;
}

char* get_http_method_string(int http_method) 
{
  switch(http_method) {
    case HTTP_METHOD_GET:
      return "GET";
    case HTTP_METHOD_POST:
      return "POST";
    case HTTP_METHOD_HEAD:
      return "HEAD";
    case HTTP_METHOD_PUT:
      return "PUT";
    case HTTP_METHOD_DELETE:
      return "DELETE";
    case HTTP_METHOD_TRACE:
      return "TRACE";
    case HTTP_METHOD_OPTIONS:
      return "OPTIONS";
    case HTTP_METHOD_CONNECT:
      return "CONNECT";
    case HTTP_METHOD_PATCH:
      return "PATCH";
    case HTTP_METHOD_INVALID:
    default:
      return "Invalid";
  }
}
int get_http_method(char* method)
{
    if(strncmp(method, "GET", 3)==0) {
        return HTTP_METHOD_GET;
    }
    else if(strncmp(method, "HEAD", 4)==0) {
        return HTTP_METHOD_HEAD;
    }
    else if(strncmp(method, "POST", 4)==0) {
        return HTTP_METHOD_POST;
    }
    else if(strncmp(method, "PUT", 3)==0) {
        return HTTP_METHOD_PUT;
    }
    else if(strncmp(method, "DELETE", 6)==0) {
        return HTTP_METHOD_DELETE;
    }
    else if(strncmp(method, "TRACE", 5)==0) {
        return HTTP_METHOD_TRACE;
    }
    else if(strncmp(method, "OPTIONS", 7)==0) {
        return HTTP_METHOD_OPTIONS;
    }
    else if(strncmp(method, "CONNECT", 7)==0) {
        return HTTP_METHOD_CONNECT;
    }
    else if(strncmp(method, "PATCH", 5)==0) {
        return HTTP_METHOD_PATCH;
    }
    return HTTP_METHOD_INVALID; //should be zero
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int end_strncmp(char* s1, char* s2, int n)
{
  int len1 = strlen(s1);
  int len2 = strlen(s2);
  if(n > len1 || n > len2)  return -1;
  return strcmp(&s1[len1 - n], &s2[len2 - n]);
}


// returns 0 on connection closed, -1 on error, -2 on full buffer
int recv_all(int sockfd, char* buf, int buf_len)
{
  int bytes_read = 0;
  int recv_status = recv(sockfd, buf, buf_len-1, 0);
  printf("recv_status = %d\n", recv_status);
  return recv_status;
  
  //While not error, or still reading bytes (to avoid infinite loop)
  while(recv_status > 0) {
    if(end_strncmp(buf, CRLF CRLF, 4) == 0) {
      return recv_status; 
    }
    bytes_read += recv_status;

    if((buf_len -1 - bytes_read) == 0) { 
      // If buffer is full, return -2 to signify full buffer
      return -2;
    }
    recv_status = recv(sockfd, buf, buf_len - 1 - bytes_read, 0);
  }

  // Return with either -1 (error) or with 0 (connection closed)
  return recv_status;
}

//Callback function for SIGCHLD signals
void sigchld_handler(int s)
{
  int saved_errno = errno;
  while(waitpid(-1, 0, WNOHANG) > 0);
  errno = saved_errno;
}

int main(int argc, char* argv[])
{
  if(argc != 2) {
    printf("usage: %s <port number>\n", argv[0]);
    exit(0);
  }
  printf("server: started\n");
  printf("server: attempting to listen on port %s\n", PORT);

  //setup hints
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);  // make sure struct is empty
  hints.ai_family = AF_UNSPEC;      // don't care IPv4 or v6
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;      // fill in IP for me
  
  //Get addrinfo linked list
  struct addrinfo *servinfo;
  int status = getaddrinfo(0, PORT, &hints, &servinfo);
  if(status != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(status);
  }
  
  //Create socket() and bind() to first one possible
  int sockfd;
  struct addrinfo* p;
  for(p = servinfo; p != 0; p = p->ai_next) {
    //Try to create socket
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if(sockfd == -1) {
      perror("server: socket");
      continue;
    }

    //Try to set socket to be rebindable from previous binding
    int yes = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    //Try to bind() to socket
    if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: could not bind");
      continue;
    }
    break; //Success!
  }
  freeaddrinfo(servinfo); //free linked list
  
  if(p == 0) { 
    //looped through linked list and failed to find socket to bind() to
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }


  //Try to setup binded socket to listen
  if(listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  //  Set callback function for SIGCHLD signals to 
  //  reap all zombie child processes
  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if(sigaction(SIGCHLD, &sa, 0) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf("server: waiting for connections...\n");
  while(1) {
    //Try to accept new connection
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);
    int new_fd = accept(sockfd, (struct sockaddr*) &their_addr, &addr_size);
    if(new_fd == -1) {
      perror("accept");
      continue;
    }

    //Get connected client IP address
    char string_addr[INET6_ADDRSTRLEN];
    inet_ntop(their_addr.ss_family, 
              get_in_addr((struct sockaddr*) &their_addr),
              string_addr, sizeof(string_addr));
    printf("server: recieved connection from %s\n", string_addr);

    if(!fork()) { 
      //Child process
      close(sockfd);  //Child doesn't need access to the listener

      char request_buffer[REQUEST_BUFFER_SIZE];
      if(recv_all(new_fd, request_buffer, REQUEST_BUFFER_SIZE) < 0) {
        send_http_error_response(new_fd, 500);
        exit(1);
      }
      

      /* Check if bad request before processing */

      // Check if too small to be a valid http request
      if(strlen(request_buffer) <= 12) {
        printf("server: recieved bad request\n"); 
        send_http_error_response(new_fd, 400);
        exit(0);
      }

      struct http_request request;
      struct pair parameters[20];

      /** Process HTTP Request Header **/
      char* current_line;

      //Get request method
      current_line = strtok(request_buffer, " ");
      request.method = get_http_method(current_line);

      //Get and seperate URI and Query string
      current_line = strtok(0, " ");
      request.uri = current_line;
      request.query = strchr(request.uri, '?');
      //if there is a query string, seperate from URI string
      if(request.query != 0) { 
                request.query[0] = '\0';
                request.query = request.query + 1;
      }
      
      //Get HTTP version
      current_line = strtok(0, "\r\n");
      request.version = current_line;

      printf("method: %d\nuri: %s\nquery: %s\nversion: %s\n",
              request.method, request.uri, request.query, request.version);
      
      request.parameters_length = 0;
      //TODO: Make this better, leaves space at begining of value
      for(int i = 0; current_line != 0 && i < 20; i++) {
        current_line = strtok(0, "\r\n:");
        request.parameters[i].name = current_line;
        current_line = strtok(0, "\r\n");
        request.parameters[i].value = current_line;/*
        printf("%s =%s\n", request.parameters[i].name, 
               request.parameters[i].value);*/
        request.parameters_length++;
      }
      /* End Processing HTTP Request */

      /* Check validity of processed HTTP Request */
      if(request.method == HTTP_METHOD_INVALID || 
          !is_valid_http_version(request.version)) {
        
        printf("Recieved bad request.\n");
        send_http_error_response(new_fd, 400);
        exit(0);
      }


      if(request.method == HTTP_METHOD_GET) {
        // Set resource_path to open file from current directory while taking
        // care in case request.uri is larger than resource_path
        char resource_path[1500] = {0};
        resource_path[0] = '.';
        strncat(resource_path, request.uri, sizeof(resource_path)-1);
        resource_path[sizeof(resource_path)-1] = '\0';

        printf("Trying to open file: %s\n", resource_path);
        int fd = open(resource_path, O_RDONLY, S_IREAD);
        if(fd == -1) {
          if(errno == ENOENT) {
            //no such file or directory
            printf("errno == ENOENT\n");
          }
          send_http_error_response(new_fd, 404);
          perror("open");
          exit(1);
        }

        //Create memory for addl headers
        struct pair header_pairs[MAX_ADDL_HEADERS];
        struct headers headers = {header_pairs, 0, MAX_ADDL_HEADERS};
        
        

        struct stat filestat;
        if(fstat(fd, &filestat) == -1) {
          perror("fstat");
          exit(1);
        }
        

        // TODO: Finish implementation of the following
        // This code handles what happens if the file opened is not a regular
        // file. If it is a directory, it should see if a index.html or home.hml
        // file exist and open that, if not, it can try to find what files exist
        // in the current directory, and list those on a html page it generates.
        
        //Check if file descriptor does not point to a file
        if(!S_ISREG(filestat.st_mode)) { 
          close(fd);
          //Check if points to directory
          if(S_ISDIR(filestat.st_mode)) {

            //Check if /index.html or /home.html exists and 301 redirect w/ '/'
            if(resource_path[strlen(resource_path)-1] != '/') {
              
              //Check for /index.html first
              strcat(resource_path, "/index.html");
              if(stat(resource_path, &filestat) == -1) {
                if(errno != ENOENT) {
                  send_http_error_response(new_fd, 500);
                  exit(1);
                }
                //No /index.html, try /home.html
                int pos = strlen(resource_path) - strlen("/index.html");
                resource_path[pos] = '\0';
                strcat(resource_path, "/home.html");

                if(stat(resource_path, &filestat) == -1) {
                  if(errno == ENOENT) {
                    send_http_error_response(new_fd, 404);
                    exit(0);
                  }
                  send_http_error_response(new_fd, 500);
                  exit(1);
                }
                //Otherwise, able to find /home.html, prep for 301, and continue
                resource_path[strlen(resource_path)-strlen("home.html")] = '\0';
              }
              else {
                //Yes, /index.html exists, prepwork for redirect
                resource_path[strlen(resource_path)-strlen("index.html")] = '\0';
              }

              //Verify it is a file (might not be)
              if(S_ISREG(filestat.st_mode)) {
                //Build and send 301 redirect
                insert_header(&headers, "Location", &resource_path[1]);
                char response_buffer[RESPONSE_BUFFER_SIZE] = {0};
                int msgsize = build_http_response_header(response_buffer, 
                                                         RESPONSE_BUFFER_SIZE,
                                                         0,
                                                         filestat.st_size,
                                                         301, 
                                                         &headers);
                send_all(new_fd, response_buffer, msgsize);
                exit(0);                                 
              }
              //Not a file, so file does not exist
              send_http_error_response(new_fd, 404);
              exit(0);
            }

            //Check if index.html exists
            strcat(resource_path, "index.html");
            fd = open(resource_path, O_RDONLY, S_IREAD);
            if(fd == -1) {
              if(errno != ENOENT) {
                send_http_error_response(new_fd, 500);
                exit(1);
              }
              int pos = strlen(resource_path) - strlen("index.html");
              resource_path[pos] = '\0';
              strcat(resource_path, "home.html");
              fd = open(resource_path, O_RDONLY, S_IREAD);
              if(fd == -1) {
                if(errno != ENOENT) {
                  send_http_error_response(new_fd, 500);
                  exit(1);
                }
                printf("No index.html or home.html\n");
                //No index or home.html, display the directory in ul
                int pos = strlen(resource_path) - strlen("home.html");
                resource_path[pos] = '\0';

                DIR* dir = opendir(resource_path);
                if(dir == 0) {
                  perror("opendir()");
                  send_http_error_response(new_fd, 500);
                  exit(1);
                }

                struct dirent *ep;
                int num_files = 0;
                int dirslen = 0;
                while((ep = readdir(dir))) {
                  num_files++;
                  dirslen += strlen(ep->d_name);
                }
                
                int msgs = STRLEN(DIRECTORY_HTML_BEGIN) + 
                           num_files * STRLEN(DIRECTORY_HTML_OPEN_LINK 
                                      DIRECTORY_HTML_CLOSE_LINK 
                                      DIRECTORY_HTML_CLOSE_ITEM)  +
                           2 * dirslen + 
                           STRLEN(DIRECTORY_HTML_END);

                char response_buffer[RESPONSE_BUFFER_SIZE] = {0};
                int msgsize = build_http_response_header(response_buffer, 
                                                         RESPONSE_BUFFER_SIZE,
                                                         "text/html",
                                                         msgs,
                                                         200, 
                                                         &headers);

                strcat(response_buffer, DIRECTORY_HTML_BEGIN);
                rewinddir(dir);
                while((ep = readdir(dir))) {
                  strcat(response_buffer, DIRECTORY_HTML_OPEN_LINK);
                  strcat(response_buffer, ep->d_name);
                  strcat(response_buffer, DIRECTORY_HTML_CLOSE_LINK);
                  strcat(response_buffer, ep->d_name);
                  strcat(response_buffer, DIRECTORY_HTML_CLOSE_ITEM);
                }
                strcat(response_buffer, DIRECTORY_HTML_END);
                
                send_all(new_fd, response_buffer, msgsize+msgs);
                exit(0); 
              }
            }

            //Able to open a file descriptor, check if it is a regular file
            if(fstat(fd, &filestat) == -1) {
              perror("fstat");
              exit(1);
            }
            if(!S_ISREG(filestat.st_mode)) {
              send_http_error_response(new_fd, 404);
              exit(0);
            }
            //It is a regular file, good to continue w/ filestat and fd
            insert_header(&headers, "Content-Location", &resource_path[1]);
          }
          //Does not point to directory, does not exist
          else {
            send_http_error_response(new_fd, 404);
            exit(0);
          }
        }
        

        //Set Content-Type
        char* response_content_type;
        if(end_strncmp(resource_path, ".html", 5) == 0) {
          response_content_type = "text/html";
        }
        else if(end_strncmp(resource_path, ".css", 4) == 0) {
          response_content_type = "text/css";
        }
        else if(end_strncmp(resource_path, ".pdf", 4) == 0) {
          response_content_type = "application/pdf";
        }
        else {
          response_content_type = "text/plain";
        }

        // Add headers to buffer, fill rest of buffer with resource
        // send, than keep loading into the buffer and sending
        char response_buffer[RESPONSE_BUFFER_SIZE] = {0};
        int header_offset = build_http_response_header(response_buffer, 
                                                       RESPONSE_BUFFER_SIZE,
                                                       response_content_type,
                                                       filestat.st_size,
                                                       200, 
                                                       &headers);



        int read_bytes = read(fd, &response_buffer[header_offset], 
                              RESPONSE_BUFFER_SIZE - header_offset);
        if(read_bytes == -1) {
          perror("file read");
          exit(1);
        }
        if(send_all(new_fd, response_buffer, header_offset+read_bytes) == -1) {
          exit(1);
        }

        //Make sure to send anything left over
        while(1) {
          read_bytes = read(fd, response_buffer, RESPONSE_BUFFER_SIZE);
          if(read_bytes == -1) {
            perror("file read");
            exit(1);
          }
          if(read_bytes == 0) { //File done reading
            break;
          }
          if(send_all(new_fd, response_buffer, read_bytes) == -1) {
            exit(1);
          }
        }
        exit(0);
      }
      //Not HTTP GET, so not implemented
      else {
        send_http_error_response(new_fd, 501);
      } 
    }

    printf("\n");
    close(new_fd); //Parent doesn't need to keep this open
  }

  return 0;
}
