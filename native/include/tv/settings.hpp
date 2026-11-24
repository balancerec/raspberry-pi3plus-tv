#pragma once
#include <filesystem>
#include <set>
#include <string>
namespace raspberry_tv {
struct UserSettings {
    std::string playlist, lastChannel, lastGroup="TODOS";
    double volume=0.65;
    bool muted=false, fullscreen=true, showOsd=true, resumeSelection=true;
    std::string audioSink="autoaudiosink", audioDevice;
};
struct UserState {UserSettings settings;std::set<std::string> favorites;};
std::filesystem::path defaultStateDirectory();
bool loadUserState(const std::filesystem::path& directory,UserState& state,std::string& error);
bool saveUserState(const std::filesystem::path& directory,const UserState& state,std::string& error);
bool validAudioSink(const std::string& value);
} // namespace raspberry_tv
