

#define HTTP_404_HTML "<!DOCTYPE html><html><style>h1{font-family:\"Helvetica\",Sans-Serif;}h1{background-color:#073642;color: white;padding: 5px;}</style><h1>404 Not Found</h1><p>The requested url could not be found on this server</p><hr><p>"SERVER_NAME"/"SERVER_VERSION"</p></html>"
#define HTTP_400_HTML "<!DOCTYPE html><html><style>h1{font-family:\"Helvetica\",Sans-Serif;}h1{background-color:#073642;color: white;padding: 5px;}</style><h1>400 Bad Request</h1><p>Server recieved bad request</p><hr><p>"SERVER_NAME"/"SERVER_VERSION"</p></html>"
#define HTTP_500_HTML "<!DOCTYPE html><html><h1>500 Internal Service Error</h1></html>"


#define DIRECTORY_HTML_BEGIN "<!DOCTYPE html><html><style>h1 {font-family: \"Helvetica\",Sans-Serif;}h1{background-color:#073642;color: white;padding: 5px;}</style><h1>Directory</h1><p>Directory listing for /site/home/test</p><ul>"
#define DIRECTORY_HTML_OPEN_LINK "<li><a href=\""
#define DIRECTORY_HTML_CLOSE_LINK "\">"
#define DIRECTORY_HTML_CLOSE_ITEM "</a></li>"
#define DIRECTORY_HTML_END "</ul><hr><p>" SERVER_NAME "/" SERVER_VERSION "</p></html>"