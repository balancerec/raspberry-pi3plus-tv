#include "tv/state_writer.hpp"
namespace raspberry_tv {
StateWriter::~StateWriter() {
    {std::lock_guard<std::mutex> lock(mutex_);stopping_=true;wake_.notify_one();}
    worker_.join();
}
void StateWriter::submit(UserState state) {std::lock_guard<std::mutex> lock(mutex_);pending_=std::move(state);wake_.notify_one();}
std::string StateWriter::takeError() {std::lock_guard<std::mutex> lock(mutex_);auto e=std::move(error_);error_.clear();return e;}
void StateWriter::run() {
    for(;;) {
        std::optional<UserState> next;
        {std::unique_lock<std::mutex> lock(mutex_);wake_.wait(lock,[this]{return stopping_ || pending_.has_value();});
        next=std::move(pending_);pending_.reset();if(!next && stopping_)return;}
        std::string error;
        try {saveUserState(path_,*next,error);}catch(...){error="Falha ao salvar configuracao";}
        if(!error.empty()){std::lock_guard<std::mutex> lock(mutex_);error_=error;}
    }
}
}
