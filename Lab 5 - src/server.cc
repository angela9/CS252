/**
 * This file contains the primary logic for your server. It is responsible for
 * handling socket communication - parsing HTTP requests and sending HTTP responses
 * to the client. 
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <fstream>
#include <map>
#include <thread>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <chrono>

#include <functional>
#include <iostream>
#include <sstream>
#include <vector>
#include <tuple>

#include "server.hh"
#include "http_messages.hh"
#include "errors.hh"
#include "misc.hh"
#include "routes.hh"

extern std::string ip;

struct string_struct {
  std::string name;
  std::string min_url;
  std::string max_url;
};
struct string_struct a;

time_t server_start;
time_t server_end;
int num_requests = 0;
double min_diff = 999999;
double max_diff = 0;

Server::Server(SocketAcceptor const& acceptor) : _acceptor(acceptor) {
  server_start = time(nullptr);
  a.name = "Angela Bansal";
}

void Server::run_linear() const {
  while (1) {
    Socket_t sock = _acceptor.accept_connection();
    handle(sock);
  }
}

void Server::run_fork() const {
  // TODO: Task 1.4
  while(1) {
    Socket_t sock = _acceptor.accept_connection();
    if (sock != NULL) {
	int ret = fork();
	if (ret == 0) {
	  handle(sock);
	  exit(0);
	}
	else if (ret < 0) {
	  perror ("fork");
	  return;
	}
	waitpid (ret, NULL, 0);
    }
  }
}

void Server::run_thread() const {
  // TODO: Task 1.4
  while(1) {
    Socket_t sock = _acceptor.accept_connection();
    if (sock != NULL) {
	std::thread child(&Server::handle, this, std::move(sock));
	child.detach();
    }
  }
}

void Server::run_thread_pool(const int num_threads) const {
  // TODO: Task 1.4
  for (int i = 0; i <num_threads; i++) {
    std::thread mythr(&Server::run_linear, this);
    mythr.detach();
  }
  Server::run_linear();
}

// example route map. you could loop through these routes and find the first route which
// matches the prefix and call the corresponding handler. You are free to implement
// the different routes however you please
/*
std::vector<Route_t> route_map = {
  std::make_pair("/cgi-bin", handle_cgi_bin),
  std::make_pair("/", handle_htdocs),
  std::make_pair("", handle_default)
};
*/

void parse_request(const Socket_t& sock, HttpRequest* const request) {
  num_requests++;
  std::string request_line = sock->readline();
  if (request_line.length() == 0) {
    request->method = "empty";
    return;
  } 
  size_t pos = 0;
  std::string token;
  int i = 0;
  while (((pos = request_line.find(' ')) != std::string::npos)) {
    token = request_line.substr(0, pos);
    request_line.erase(0, pos+1);
    if (i == 0) {
	request->method = token;
    }
    else if (i == 1) {
	request->request_uri = token;
    }
    else if (i == 2) {
	
    }    
    i++;
  }
  pos = request_line.find("\r\n");
  request_line.erase(pos, 2);
  request->http_version = request_line;
  while (request_line.size() != 2) {
    request_line = sock->readline();
    pos = request_line.find(":");
    if (pos != std::string::npos) {
	std::string key = request_line.substr(0, pos);
	std::string val = request_line.substr(pos+2, request_line.length());
	request->headers[key] = val;
    }
  }
}

void Server::handle(const Socket_t& sock) const {
  auto start_time = std::chrono::system_clock::now();
  HttpRequest request;
  // TODO: implement parsing HTTP requests
  // recommendation:
  // void parse_request(const Socket_t& sock, HttpRequest* const request);
  HttpResponse resp;
  parse_request(sock, &request);
  if (request.method.find("empty") != std::string::npos) {
    std::cout << "HTTP/1.1 200 OK" << std::endl;
    std::cout << "Connection: close" << std::endl;
    return;
  }
  bool cgi = false;
  if (request.request_uri.length() > 7 && request.request_uri.substr(0, 5).compare("/cgi-") == 0) {
    cgi = true;
    if (request.request_uri.find("?") != std::string::npos) {
	request.query = request.request_uri.substr(request.request_uri.find("?")+1, request.request_uri.length());
	request.request_uri = request.request_uri.substr(0, request.request_uri.find("?"));
    }
    resp = handle_cgi_bin(request);
    std::stringstream sss;
    sss << "HTTP/1.1 200 OK\r\n";
    sss << "Connection: close\r\n";
    sss << resp.message_body;
    sock->write(sss.str());
    return;
  }
  resp.http_version = request.http_version;
  if (resp.http_version.compare("HTTP/1.1") != 0 && resp.http_version.compare("HTTP/1.0") != 0) {
    resp.status_code = 400;
    std::cout << resp.to_string() << std::endl;
    sock->write(resp.to_string());
    return;
  }
  if (request.headers.find("Authorization") == request.headers.end()) {
    resp.status_code = 401;
    std::cout << resp.to_string() << std::endl;
    sock->write(resp.to_string());
    return;
  }
  else {
    std::string checker = request.headers.find("Authorization")->second;
    if (checker.find("YWRtaW46YWRtaW4=") == std::string::npos) {
	resp.status_code = 401;
	std::cout << resp.to_string() << std::endl;
	sock->write(resp.to_string());
	return;	
    }
  }
  if (request.request_uri.compare("/stats") == 0) {
    std::stringstream ss;
    ss << "HTTP/1.1 200 OK" << "\r\n";
    ss << "Connection: close" << "\r\n";
    ss << "\r\n";
    ss << "Name: " << a.name << "\r\n";
    server_end = time(nullptr);
    double diff = difftime(server_end, server_start);
    ss << "Uptime: " << diff << "s\r\n";
    ss << "Number of requests: " << num_requests << "\r\n";
    ss << "Maximum Service Time: " << max_diff << "s URL: " << a.max_url << "\r\n";
    ss << "Minumum Service Time: " << min_diff << "s URL: " << a.min_url << "\r\n";
    sock->write(ss.str());
    return;
  }
  else if (request.request_uri.compare("/logs") == 0) {
    FILE * ff = fopen("myhttpd.log", "r");
    std::stringstream ss;
    std::ifstream ifs("myhttpd.log");
    std::string logs((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ss << "HTTP/1.1 200 OK" << "\r\n";
    ss << "Connection: close" << "\r\n";
    ss << "Content-Length: " << logs.length() << "\r\n";
    ss << "Content-Type: " << get_content_type("myhttpd.log") << "\r\n";
    ss << "\r\n";
    ss << logs;
    sock->write(ss.str());
  }
  else if (!cgi) {
    if (request.request_uri.back() == '/') {
	request.request_uri += "index.html";
    }
    struct stat sb;
    std::string path = "http-root-dir/htdocs" + request.request_uri;
    stat(path.c_str(), &sb);
    if (S_ISDIR(sb.st_mode)) {
	if (path.back() != '/') {
	  path += "/";
	  request.request_uri += "/";
    	}
	if (path.back() == '/') {
      	  DIR * dd = opendir(path.c_str());
          if (NULL == dd) {
            perror("opendir");
            exit(1);
          }
	  std::stringstream ss;
          ss << "HTTP/1.1 200 OK" << "\r\n";
          ss << "Connection: close" << "\r\n";
          ss << "Content-Type: text/html;charset=us-ascii\r\n";
          ss << "\r\n";
	  std::string dirname;
          std::string dirp;
	  for (dirent * ent = readdir(dd); NULL != ent; ent = readdir(dd)) {
            dirname = ent->d_name;
            dirp = path + dirname;
	    stat(dirp.c_str(), &sb);
            if (S_ISDIR(sb.st_mode)) {
          	ss << "<li><A HREF=\"" + request.request_uri + dirname + "/\">" + dirname + " </A>\n";
            }
	    else {
		ss << "<li><A HREF=\"" + request.request_uri + dirname + "\">" + dirname + " </A>\n";
	    }
    	  }
	  sock->write(ss.str());
          return;
	}
    }
    std::ifstream ifs(path);
    if (ifs.good()) {
      std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
      request.message_body = content;
      resp.status_code = 200;
    }
    else {
      resp.status_code = 404;
    }
    request.print();
  // TODO: Make a response for the HTTP request
    resp.headers["Content-Length"] = std::to_string(request.message_body.length());
    resp.headers["Content-Type"] = get_content_type(path);
    resp.message_body = request.message_body;
  }
  request.print();
  std::cout << resp.to_string() << std::endl;
  sock->write(resp.to_string());
  auto end_time = std::chrono::system_clock::now();
  std::chrono::duration<double> diff = end_time - start_time;
  if (diff.count() < min_diff) {
    min_diff = diff.count();
    a.min_url = request.request_uri;
  }
  if (diff.count() > max_diff) {
    max_diff = diff.count();
    a.max_url = request.request_uri;
  }
  FILE * fd;
  fd = fopen("myhttpd.log", "a+");
  if (ip.length() > 0) {
    fprintf(fd, "Source IP:%s | Route:%s | Response Code:%d\n", ip.c_str(), request.request_uri.c_str(), resp.status_code);
  }
  fclose(fd);
}
