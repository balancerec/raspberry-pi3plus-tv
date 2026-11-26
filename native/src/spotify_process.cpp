#include "tv/spotify.hpp"
#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <utility>

namespace raspberry_tv {
namespace {
const char* failure="RTVSPOTIFY\t1\nstatus\terror\tCliente%20Spotify%20indisponivel%3B%20verifique%20Python3%20e%20o%20pacote.\n";
// All strings/argv are prepared before fork. The child uses only async-signal-safe
// syscalls before exec, important in the SDL/GStreamer multithreaded process.
std::string run(const std::string& helper,const SpotifyArguments& arguments,const SpotifyCancel& cancel) {
    std::vector<std::string> owned{"python3","-I",helper};
    owned.insert(owned.end(),arguments.begin(),arguments.end());
    std::vector<char*> argv;for(auto& value:owned)argv.push_back(value.data());argv.push_back(nullptr);
    int channel[2];if(pipe2(channel,O_CLOEXEC)!=0)return failure;
    int discard=open("/dev/null",O_WRONLY|O_CLOEXEC);
    if(discard<0){close(channel[0]);close(channel[1]);return failure;}
    const auto child=fork();
    if(child==0) {
        if(setpgid(0,0)!=0)_exit(126);
        if(dup2(channel[1],STDOUT_FILENO)<0 || dup2(discard,STDERR_FILENO)<0)_exit(126);
        close(channel[0]);close(channel[1]);close(discard);
        execvp(argv[0],argv.data());_exit(127);
    }
    close(channel[1]);close(discard);
    if(child<0){close(channel[0]);return failure;}
    setpgid(child,child); // Handles cancellation before the child's first syscall.
    fcntl(channel[0],F_SETFL,O_NONBLOCK);
    std::string output;bool ended=false,aborted=false;int status=0;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!ended) {
        if(cancel->load() || std::chrono::steady_clock::now()>=deadline){aborted=true;break;}
        pollfd descriptor{channel[0],POLLIN,0};
        if(poll(&descriptor,1,20)<0 && errno!=EINTR){aborted=true;break;}
        char buffer[4096];ssize_t count;
        while((count=read(channel[0],buffer,sizeof(buffer)))>0) {
            output.append(buffer,static_cast<std::size_t>(count));
            if(output.size()>262144){aborted=true;break;}
        }
        if(aborted)break;
        const auto waited=waitpid(child,&status,WNOHANG);
        if(waited==child)ended=true;
        else if(waited<0 && errno!=EINTR){aborted=true;break;}
    }
    if(aborted) {
        kill(-child,SIGKILL);kill(child,SIGKILL);while(waitpid(child,&status,0)<0 && errno==EINTR){}
        output="RTVSPOTIFY\t1\nstatus\tnetwork\tOperacao%20Spotify%20cancelada%20ou%20tempo%20limite%20excedido.\n";
    } else {
        char buffer[4096];ssize_t count;
        while((count=read(channel[0],buffer,sizeof(buffer)))>0 && output.size()<=262144)output.append(buffer,static_cast<std::size_t>(count));
        // The helper returns exit 1 for a structured, safe user error.
        if(!WIFEXITED(status) || WEXITSTATUS(status)>1 || output.empty())output=failure;
    }
    close(channel[0]);return output;
}
}
SpotifyRunner makeSpotifyRunner(const std::string& helper) {
    return [helper](const SpotifyArguments& arguments,const SpotifyCancel& cancel){return run(helper,arguments,cancel);};
}
} // namespace raspberry_tv
