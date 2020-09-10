#include <unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include "http_messages.hh"
#include <dlfcn.h>
#include <link.h>
#include <errno.h>

// You could implement your logic for handling /cgi-bin requests here
typedef void (*httprunfunc)(int ssock, const char *querystring);

HttpResponse handle_cgi_bin(const HttpRequest& request) {
  HttpResponse response;
  response.http_version = request.http_version;
  // TODO: Task 2.2;
  return response;
}
