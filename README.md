# http-c-server
This is a basic HTTP web server not intended for any type of serious use. It was primarily built as a learning tool for myself to investigate socket programming, HTTP protocol, and C POSIX.

### What is currently working?
Currently it supports processing HTTP Requests, checking their validity, and returning a proper HTTP response.

The only HTTP method currently supported is GET. For all other HTTP request methods, a HTTP 501 response page is returned.
Currently, when the request uri specifies a directory, the server checks if a index.html or home.html file exists in the directory, and serves that if it does. 
If the request is for a directory with no index or home html page, then the server will return a html page listing the files in the current directory.

The current method for serving clients is to ``` listen() ``` on parent process, and for each accepted connection, ``` fork() ``` a child process to serve the files over HTTP.


### Things to add
- Prevent client from being able to access above running directory with malcious GET requests
- Look into using ``` epoll() and kqueue()``` to find a faster way to serve files
- Switch to creating worker processes capable of serving multiple clients nonsynchronously
- Add ability to check content types from a mime.types file (Currently hardcoded in)
- Test the server's performance (Just for fun! Might be nice to keep track of and know)
