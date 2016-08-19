# http-c-server
This is a basic HTTP web server not intended for any type of serious use. It was primarily built as a learning tool for myself to investigate socket programming, HTTP protocol, and C POSIX.

### What is currently working?
Currently it supports processing HTTP Requests, checking their validity, and returning a proper HTTP response.

The only HTTP method currently supported is GET. For all other HTTP request methods, a HTTP 501 response page is returned.
Currently, when the request uri specifies a directory, the server checks if a index.html file exists in the directory, and serves that if it does.

The current method for serving clients is to ``` listen() ``` on parent process, and for each accepted connection, ``` fork() ``` a child process to serve the files over HTTP.


### Things to add
- Check if the request uri is a directory, if there is no index.html or home.html, list out the directory contents in a  ``` <ul> ```
- Prevent client from being able to access above running directory with malcious GET requests
- Look into using ``` select() ``` and NONBLOCKING mode on sockets to find a faster way to serve files
- Add ability to check content types from a mime.types file (Currently hardcoded in)
- Test the server's performance (Just for fun! Might be nice to keep track of and know)
