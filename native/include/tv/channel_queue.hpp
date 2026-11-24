#pragma once
#include "tv/menu.hpp"
namespace raspberry_tv {
// Snapshot the current filtered list as indices; the catalog cannot reload while playing.
class ChannelQueue {
public:
    void start(const TvModel& model) {
        indices_.clear();favorites_=model.view()==View::Favorites;cursor_=model.selectedRow();
        indices_.reserve(model.visibleCount());
        for(std::size_t i=0;i<model.visibleCount();++i)indices_.push_back(model.channelIndexAt(i));
    }
    const Channel* current(const TvModel& model) const {
        if(cursor_>=indices_.size() || indices_[cursor_]>=model.channels().size())return nullptr;
        return &model.channels()[indices_[cursor_]];
    }
    bool step(int direction,const TvModel& model) {
        if(!direction || indices_.size()<2)return false;
        for(std::size_t offset=1;offset<indices_.size();++offset) {
            const auto next=(cursor_+(direction>0?offset:indices_.size()-offset))%indices_.size();
            if(favorites_ && !model.hasFavorite(indices_[next]))continue;
            cursor_=next;return true;
        }
        return false;
    }
private:
    std::vector<std::size_t> indices_;std::size_t cursor_=0;bool favorites_=false;
};
}
