#include "tv/player.hpp"
#include "tv/playlist.hpp"
#include <SDL.h>
#include <SDL_syswm.h>
#include <gst/video/videooverlay.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
namespace raspberry_tv {
std::uint64_t TvPlayer::now() {return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
TvPlayer::~TvPlayer(){shutdown();}
bool TvPlayer::initialize(SDL_Window* window,const UserSettings& settings) {
    if(ready_ || !window)return false;
    SDL_SysWMinfo info{};SDL_VERSION(&info.version);
    if(!SDL_GetWindowWMInfo(window,&info) || info.subsystem!=SDL_SYSWM_X11)return false;
    windowHandle_=static_cast<guintptr>(info.info.x11.window);
    if(!windowHandle_ || !validAudioSink(settings.audioSink))return false;
    settings_=settings;snapshot_.volume=settings.volume;snapshot_.muted=settings.muted;
    exit_=false;headless_=false;desiredPause_=false;audioDirty_=false;pending_=Command::NoRequest;machine_.stopped();snapshot_.state=PlaybackState::Idle;
    ready_=true;worker_=std::thread(&TvPlayer::run,this);return true;
}
bool TvPlayer::initializeHeadless() {
    if(ready_)return false;
    headless_=true;exit_=false;desiredPause_=false;audioDirty_=false;pending_=Command::NoRequest;machine_.stopped();snapshot_.state=PlaybackState::Idle;
    ready_=true;worker_=std::thread(&TvPlayer::run,this);return true;
}
PlayerSnapshot TvPlayer::snapshot() const {std::lock_guard<std::mutex> lock(mutex_);return snapshot_;}
bool TvPlayer::play(const std::string& uri) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!ready_ || !allowedMediaUri(uri)) {snapshot_.status="Player indisponivel ou URI invalida";return false;}
    ++generation_;pending_=Command::Play;pendingUri_=uri;desiredPause_=false;
    snapshot_.state=PlaybackState::Opening;snapshot_.status="Abrindo canal...";snapshot_.buffering=0;snapshot_.audioStreams=-1;
    wake_.notify_one();return true;
}
void TvPlayer::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!ready_)return;
    ++generation_;pending_=Command::Stop;pendingUri_.clear();desiredPause_=false;
    snapshot_.state=PlaybackState::Stopping;snapshot_.status="Encerrando reproducao...";wake_.notify_one();
}
void TvPlayer::shutdown() {
    {std::lock_guard<std::mutex> lock(mutex_);if(!ready_)return;
    ++generation_;pending_=Command::Shutdown;pendingUri_.clear();wake_.notify_one();}
    if(worker_.joinable())worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);ready_=false;windowHandle_=0;snapshot_.state=PlaybackState::Shutdown;
}
void TvPlayer::togglePause() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(snapshot_.state!=PlaybackState::Playing && snapshot_.state!=PlaybackState::Paused && snapshot_.state!=PlaybackState::Buffering)return;
    desiredPause_=!desiredPause_;wake_.notify_one();
}
void TvPlayer::changeVolume(double delta) {
    if(!std::isfinite(delta))return;
    std::lock_guard<std::mutex> lock(mutex_);snapshot_.volume=std::clamp(snapshot_.volume+delta,0.0,1.0);audioDirty_=true;wake_.notify_one();
}
void TvPlayer::toggleMute() {std::lock_guard<std::mutex> lock(mutex_);snapshot_.muted=!snapshot_.muted;audioDirty_=true;wake_.notify_one();}
void TvPlayer::publish(const std::string& status) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(currentGeneration_!=generation_)return; // an obsolete pipeline cannot overwrite a newer request
    snapshot_.state=machine_.state();snapshot_.buffering=machine_.percent();
    if(!status.empty())snapshot_.status=status;
}
void TvPlayer::releasePipeline() {
    if(playbin_)gst_element_set_state(playbin_,GST_STATE_NULL);
    if(bus_){gst_object_unref(bus_);bus_=nullptr;}
    if(playbin_){gst_object_unref(playbin_);playbin_=nullptr;}
    live_=false;buffering_=false;paused_=false;
}
void TvPlayer::applyAudio() {
    double volume;bool mute;
    {std::lock_guard<std::mutex> lock(mutex_);volume=snapshot_.volume;mute=snapshot_.muted;audioDirty_=false;}
    if(playbin_)g_object_set(playbin_,"volume",volume,"mute",mute?TRUE:FALSE,nullptr);
}
namespace {
// Source properties vary by plugin; only set an exactly matching type.
void uintProperty(GObject* object,const char* name,guint value) {
    GParamSpec* p=g_object_class_find_property(G_OBJECT_GET_CLASS(object),name);
    if(p && (p->flags&G_PARAM_WRITABLE) && G_PARAM_SPEC_VALUE_TYPE(p)==G_TYPE_UINT)g_object_set(object,name,value,nullptr);
}
void sourceSetup(GstElement*,GstElement* source,gpointer) {
    uintProperty(G_OBJECT(source),"timeout",10); // souphttpsrc: seconds
    uintProperty(G_OBJECT(source),"retries",0); // app owns bounded retry policy
    if(auto* factory=gst_element_get_factory(source)) {
        const std::string name=gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
        if(name=="udpsrc")g_object_set(source,"timeout",static_cast<guint64>(15*GST_SECOND),nullptr);
    }
    // rtspsrc uses microseconds and uint64, not souphttpsrc's seconds.
    GParamSpec* p=g_object_class_find_property(G_OBJECT_GET_CLASS(source),"tcp-timeout");
    if(p && G_PARAM_SPEC_VALUE_TYPE(p)==G_TYPE_UINT64)g_object_set(source,"tcp-timeout",static_cast<guint64>(10000000),nullptr);
}
}
bool TvPlayer::createPipeline() {
    pipelineFailure_=PlaybackFailure::Codec;
    playbin_=gst_element_factory_make("playbin","raspberry-tv-playbin");
    if(!playbin_)return false;
    gst_object_ref_sink(GST_OBJECT(playbin_));
    GstElement* video=gst_element_factory_make(headless_?"fakesink":"glimagesink",nullptr);
    GstElement* audio=gst_element_factory_make(headless_?"fakesink":settings_.audioSink.c_str(),nullptr);
    if(!video || !audio) {if(!audio)pipelineFailure_=PlaybackFailure::Audio;if(video)gst_object_unref(video);if(audio)gst_object_unref(audio);releasePipeline();return false;}
    gst_object_ref_sink(video);gst_object_ref_sink(audio);
    if(headless_)g_object_set(audio,"sync",TRUE,nullptr);
    if(!headless_) {
        if(!GST_IS_VIDEO_OVERLAY(video)) {gst_object_unref(video);gst_object_unref(audio);releasePipeline();return false;}
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(video),windowHandle_);
        gst_video_overlay_handle_events(GST_VIDEO_OVERLAY(video),FALSE);
        if(!settings_.audioDevice.empty()) {
            auto* p=g_object_class_find_property(G_OBJECT_GET_CLASS(audio),"device");
            if(!p || G_PARAM_SPEC_VALUE_TYPE(p)!=G_TYPE_STRING || !(p->flags&G_PARAM_WRITABLE)) {pipelineFailure_=PlaybackFailure::Audio;gst_object_unref(video);gst_object_unref(audio);releasePipeline();return false;}
            g_object_set(audio,"device",settings_.audioDevice.c_str(),nullptr);
        }
    }
    g_object_set(playbin_,"video-sink",video,"audio-sink",audio,nullptr);
    gst_object_unref(video);gst_object_unref(audio); // playbin owns independent references
    g_signal_connect(playbin_,"source-setup",G_CALLBACK(sourceSetup),nullptr);
    bus_=gst_element_get_bus(playbin_);
    if(!bus_){releasePipeline();return false;}
    applyAudio();return true;
}
bool TvPlayer::startStream(bool retry) {
    if(currentGeneration_!=generation_)return false;
    machine_.open(now(),retry);publish("Abrindo canal...");
    lastPosition_=-1;lastProgress_=now();lastProbe_=now();
    if(!createPipeline()){handleFailure(pipelineFailure_);return false;}
    if(currentGeneration_!=generation_){releasePipeline();return false;}
    g_object_set(playbin_,"uri",uri_.c_str(),nullptr);
    const auto change=gst_element_set_state(playbin_,GST_STATE_PLAYING);
    if(change==GST_STATE_CHANGE_FAILURE){handleFailure(PlaybackFailure::Network);return false;}
    live_=change==GST_STATE_CHANGE_NO_PREROLL;
    return true;
}
void TvPlayer::handleFailure(PlaybackFailure failure) {
    releasePipeline();
    if(currentGeneration_!=generation_)return;
    {std::lock_guard<std::mutex> lock(mutex_);if(currentGeneration_==generation_)desiredPause_=false;}
    machine_.fail(failure,now(),uri_.rfind("file://",0)!=0);
    std::string status=playbackFailureMessage(failure);
    if(machine_.state()==PlaybackState::RetryWait)status+="; nova tentativa "+std::to_string(machine_.attempts())+"/2 (O cancela)";
    publish(status);
    std::fprintf(stderr,"Player: state=%s category=%d retry=%u (endereco omitido)\n",playbackStateName(machine_.state()),static_cast<int>(failure),machine_.attempts());
}
void TvPlayer::inspectBus() {
    for(int i=0;bus_ && i<64 && currentGeneration_==generation_;++i) {
        GstMessage* msg=gst_bus_pop(bus_);if(!msg)break;
        PlaybackFailure failure=PlaybackFailure::Network;bool failed=false,eos=false;
        switch(GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError* e=nullptr;gchar* debug=nullptr;gst_message_parse_error(msg,&e,&debug);
                if(e) {
                    if(e->domain==GST_STREAM_ERROR)failure=e->code==GST_STREAM_ERROR_CODEC_NOT_FOUND || e->code==GST_STREAM_ERROR_WRONG_TYPE || e->code==GST_STREAM_ERROR_TYPE_NOT_FOUND?PlaybackFailure::Codec:PlaybackFailure::Decode;
                    else if(e->domain==GST_CORE_ERROR)failure=e->code==GST_CORE_ERROR_MISSING_PLUGIN?PlaybackFailure::Codec:PlaybackFailure::Internal;
                    else if(e->domain==GST_RESOURCE_ERROR && (e->code==GST_RESOURCE_ERROR_NOT_FOUND || e->code==GST_RESOURCE_ERROR_NOT_AUTHORIZED))failure=PlaybackFailure::Access;
                    if(e->domain==GST_RESOURCE_ERROR && e->message) {
                        // Some HTTP plugins report OPEN_READ for these statuses. Inspect,
                        // but never print/store the raw provider message or debug details.
                        const std::string detail=e->message;
                        if(detail.find("(403)")!=std::string::npos || detail.find("(404)")!=std::string::npos ||
                           detail.find("Forbidden")!=std::string::npos || detail.find("Not Found")!=std::string::npos)failure=PlaybackFailure::Access;
                    }
                    // Identify errors from the configured sink, without logging plugin messages/URLs.
                    GstElement* sink=nullptr;g_object_get(playbin_,"audio-sink",&sink,nullptr);
                    if(sink) {if(GST_MESSAGE_SRC(msg)==GST_OBJECT(sink) || gst_object_has_as_ancestor(GST_MESSAGE_SRC(msg),GST_OBJECT(sink)))failure=PlaybackFailure::Audio;gst_object_unref(sink);}
                    g_error_free(e);
                }
                g_free(debug);failed=true;break;
            }
            case GST_MESSAGE_EOS:eos=true;break;
            case GST_MESSAGE_BUFFERING: {
                gint percent=100;gst_message_parse_buffering(msg,&percent);machine_.buffering(percent,now());
                buffering_=percent<100;
                if(!live_)gst_element_set_state(playbin_,buffering_?GST_STATE_PAUSED:(paused_?GST_STATE_PAUSED:GST_STATE_PLAYING));
                if(!buffering_ && live_ && !paused_)machine_.playing(now());
                publish(buffering_?"Carregando canal...":"Buffer completo");break;
            }
            case GST_MESSAGE_STATE_CHANGED:
                if(GST_MESSAGE_SRC(msg)==GST_OBJECT(playbin_)) {
                    GstState oldState,newState,pending;gst_message_parse_state_changed(msg,&oldState,&newState,&pending);
                    if(newState==GST_STATE_PLAYING) {
                        machine_.playing(now());publish("Reproduzindo");
                        gint n=-1;g_object_get(playbin_,"n-audio",&n,nullptr);
                        std::lock_guard<std::mutex> lock(mutex_);if(currentGeneration_==generation_)snapshot_.audioStreams=n;
                    }
                }break;
            case GST_MESSAGE_ELEMENT:
                if(const auto* structure=gst_message_get_structure(msg);structure && gst_structure_has_name(structure,"GstUDPSrcTimeout"))failed=true;
                break;
            case GST_MESSAGE_LATENCY:gst_bin_recalculate_latency(GST_BIN(playbin_));break;
            case GST_MESSAGE_CLOCK_LOST:gst_element_set_state(playbin_,GST_STATE_PAUSED);if(!paused_)gst_element_set_state(playbin_,GST_STATE_PLAYING);break;
            case GST_MESSAGE_WARNING:publish("Aviso de reproducao; verifique audio/video");break;
            default:break;
        }
        gst_message_unref(msg);
        if(failed){handleFailure(failure);return;}
        if(eos) {
            if(uri_.rfind("file://",0)==0){releasePipeline();machine_.stopped();
                {std::lock_guard<std::mutex> lock(mutex_);if(currentGeneration_==generation_)desiredPause_=false;}
                publish("Fim do arquivo");}
            else handleFailure(PlaybackFailure::Network);
            return;
        }
    }
}
void TvPlayer::run() {
    gst_init(nullptr,nullptr);
    if(auto* decoder=gst_registry_find_feature(gst_registry_get(),"v4l2h264dec",GST_TYPE_ELEMENT_FACTORY)) {
        gst_plugin_feature_set_rank(decoder,GST_RANK_PRIMARY+100);gst_object_unref(decoder);
    }
    while(!exit_) {
        Command command;std::string uri;bool pause,audio;
        {std::unique_lock<std::mutex> lock(mutex_);
        wake_.wait_for(lock,std::chrono::milliseconds(20),[this]{return pending_!=Command::NoRequest || audioDirty_ || desiredPause_!=paused_;});
        command=pending_;pending_=Command::NoRequest;uri=std::move(pendingUri_);pause=desiredPause_;audio=audioDirty_;
        if(command!=Command::NoRequest)currentGeneration_=generation_;}
        if(command==Command::Shutdown){releasePipeline();machine_.shutdown();exit_=true;continue;}
        if(command==Command::Play || command==Command::Stop) {
            machine_.stopping();releasePipeline();machine_.stopped();
            if(command==Command::Stop){uri_.clear();publish("Reproducao encerrada");continue;}
            uri_=std::move(uri);startStream(false);
        }
        if(currentGeneration_!=generation_)continue;
        if(audio)applyAudio();
        if(playbin_ && pause!=paused_) {
            paused_=pause;machine_.pause(paused_,now());
            if(gst_element_set_state(playbin_,paused_ || buffering_?GST_STATE_PAUSED:GST_STATE_PLAYING)==GST_STATE_CHANGE_FAILURE)handleFailure(PlaybackFailure::Decode);
            else publish(paused_?"Pausado":"Retomando...");
        }
        inspectBus();
        if(machine_.state()==PlaybackState::Playing && playbin_ && now()-lastProbe_>=2000) {
            lastProbe_=now();gint64 position=-1;
            if(gst_element_query_position(playbin_,GST_FORMAT_TIME,&position) && position>=0) {
                if(position!=lastPosition_){lastPosition_=position;lastProgress_=now();}
                else if(now()-lastProgress_>=20000)handleFailure(PlaybackFailure::Network);
            } else lastProgress_=now(); // non-seekable live source: rely on source timeout/bus
        } else if(machine_.state()!=PlaybackState::Playing)lastProgress_=now();
        const auto tick=machine_.tick(now());
        if(tick==PlaybackTick::Timeout)handleFailure(PlaybackFailure::Network);
        else if(tick==PlaybackTick::Retry)startStream(true);
    }
}
} // namespace raspberry_tv
