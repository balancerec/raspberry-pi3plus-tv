#pragma once
#include "test.hpp"
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <string>
// Local, bounded HTTP error server. No external network or credentials.
class HttpErrorFixture {
public:
    explicit HttpErrorFixture(int status) {
        fd_=socket(AF_INET,SOCK_STREAM,0);check(fd_>=0,"HTTP fixture socket");
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        check(bind(fd_,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0 && listen(fd_,1)==0,"HTTP fixture bind/listen");
        socklen_t size=sizeof(address);check(getsockname(fd_,reinterpret_cast<sockaddr*>(&address),&size)==0,"HTTP fixture port");
        uri="http://127.0.0.1:"+std::to_string(ntohs(address.sin_port))+"/test";
        worker_=std::thread([this,status] {
            pollfd wait{fd_,POLLIN,0};
            if(poll(&wait,1,5000)>0) {
                int client=accept(fd_,nullptr,nullptr);
                if(client>=0) {
                    // Consume the bounded GET headers before close to avoid a TCP RST.
                    pollfd request{client,POLLIN,0};char buffer[4096];
                    if(poll(&request,1,1000)>0)recv(client,buffer,sizeof(buffer),0);
                    // Do not parse/log a request; send only a fixed response.
                    const auto text="HTTP/1.1 "+std::to_string(status)+(status==403?" Forbidden":" Not Found")+"\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send(client,text.data(),text.size(),MSG_NOSIGNAL);close(client);
                }
            }
        });
    }
    ~HttpErrorFixture(){worker_.join();close(fd_);}
    std::string uri;
private:
    int fd_=-1;std::thread worker_;
};
