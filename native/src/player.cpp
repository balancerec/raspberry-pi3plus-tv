#include "tv/player.hpp"
#include <SDL.h>
#include <SDL_syswm.h>
#include <gst/video/videooverlay.h>
#include <cstdio>

namespace raspberry_tv {

TvPlayer::~TvPlayer() {
    shutdown();
}

void TvPlayer::shutdown() {
    // GstObject starts with a floating reference. The application explicitly
    // owns playbin_ until it has been stopped and the X11 overlay is gone.
    if (playbin_) {
        gst_element_set_state(playbin_, GST_STATE_NULL);
        // Bounded wait: do not hang the UI indefinitely on a damaged stream.
        gst_element_get_state(playbin_, nullptr, nullptr, GST_SECOND);
    }
    if (bus_) {
        gst_object_unref(bus_);
        bus_ = nullptr;
    }
    if (playbin_) {
        gst_object_unref(playbin_);
        playbin_ = nullptr;
    }
    active_ = false;
    paused_ = false;
    live_ = false;
    ready_ = false;
    windowHandle_ = 0;
}

bool TvPlayer::initialize(SDL_Window* window) {
    if (!window) {
        status_ = "Janela SDL nao encontrada";
        return false;
    }
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_X11) {
        status_ = "Player requer sessao SDL2/X11";
        return false;
    }
    windowHandle_ = static_cast<guintptr>(info.info.x11.window);
    if (!windowHandle_) {
        status_ = "Identificador X11 invalido";
        return false;
    }
    gst_init(nullptr, nullptr);
    ready_ = true;
    status_ = "Player GStreamer pronto";
    return true;
}

bool TvPlayer::createPipeline() {
    if (playbin_) return true;
    if (!ready_) return false;

    playbin_ = gst_element_factory_make("playbin", "raspberry-tv-playbin");
    if (!playbin_) {
        status_ = "GStreamer playbin indisponivel";
        return false;
    }
    // Claim the floating reference before any property assignments or
    // parent ownership transfers, so teardown never uses a dangling pointer.
    gst_object_ref_sink(GST_OBJECT(playbin_));
    GstElement* sink = gst_element_factory_make("glimagesink", "raspberry-tv-video");
    if (!sink) {
        status_ = "GStreamer glimagesink indisponivel";
        gst_object_unref(playbin_);
        playbin_ = nullptr;
        return false;
    }
    gst_object_ref_sink(GST_OBJECT(sink));
    if (!GST_IS_VIDEO_OVERLAY(sink)) {
        status_ = "Renderizador GL nao suporta VideoOverlay";
        gst_object_unref(sink);
        gst_object_unref(playbin_);
        playbin_ = nullptr;
        return false;
    }
    // Share the existing SDL X11 window, no new kiosk/service or root needed.
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), windowHandle_);
    gst_video_overlay_handle_events(GST_VIDEO_OVERLAY(sink), FALSE);
    g_object_set(G_OBJECT(playbin_), "video-sink", sink, nullptr);
    // playbin keeps its own reference. This releases only our explicit ref.
    gst_object_unref(sink);

    bus_ = gst_element_get_bus(playbin_);
    if (!bus_) {
        status_ = "Falha ao obter barramento GStreamer";
        gst_object_unref(playbin_);
        playbin_ = nullptr;
        return false;
    }

    // Prefer the validated Pi H.264 V4L2 decoder within this process only.
    GstPluginFeature* decoder = gst_registry_find_feature(
        gst_registry_get(), "v4l2h264dec", GST_TYPE_ELEMENT_FACTORY);
    if (decoder) {
        gst_plugin_feature_set_rank(decoder, GST_RANK_PRIMARY + 100);
        gst_object_unref(decoder);
    }
    return true;
}

bool TvPlayer::play(const std::string& uri) {
    if (!createPipeline()) return false;
    if (uri.empty() || !gst_uri_is_valid(uri.c_str())) {
        status_ = "URI do canal invalida";
        return false;
    }
    // Restrict external scheme handlers and avoid printing tokenized IPTV URLs.
    gchar* scheme = gst_uri_get_protocol(uri.c_str());
    const bool allowed = scheme &&
        (g_strcmp0(scheme, "http") == 0 ||
         g_strcmp0(scheme, "https") == 0 ||
         g_strcmp0(scheme, "rtsp") == 0 ||
         g_strcmp0(scheme, "rtmp") == 0 ||
         g_strcmp0(scheme, "udp") == 0 ||
         g_strcmp0(scheme, "file") == 0);
    g_free(scheme);
    if (!allowed) {
        status_ = "Protocolo do canal nao permitido";
        return false;
    }

    stop();
    g_object_set(G_OBJECT(playbin_), "uri", uri.c_str(), nullptr);
    desiredPlaying_ = true;
    live_ = false;
    paused_ = false;
    const GstStateChangeReturn result =
        gst_element_set_state(playbin_, GST_STATE_PLAYING);
    if (result == GST_STATE_CHANGE_FAILURE) {
        status_ = "Falha ao iniciar pipeline";
        gst_element_set_state(playbin_, GST_STATE_NULL);
        return false;
    }
    live_ = result == GST_STATE_CHANGE_NO_PREROLL;
    active_ = true;
    status_ = "Abrindo canal...";
    return true;
}

void TvPlayer::stop() {
    if (playbin_) gst_element_set_state(playbin_, GST_STATE_NULL);
    if (bus_) {
        GstMessage* msg = nullptr;
        while ((msg = gst_bus_pop(bus_)) != nullptr) gst_message_unref(msg);
    }
    active_ = false;
    paused_ = false;
    desiredPlaying_ = true;
    live_ = false;
    status_ = "Reproducao encerrada";
}

void TvPlayer::togglePause() {
    if (!active_ || !playbin_) return;
    desiredPlaying_ = !desiredPlaying_;
    paused_ = !desiredPlaying_;
    gst_element_set_state(playbin_, paused_ ? GST_STATE_PAUSED : GST_STATE_PLAYING);
    status_ = paused_ ? "Pausado" : "Reproduzindo";
}

bool TvPlayer::poll() {
    if (!bus_ || !active_) return false;
    bool changed = false;
    GstMessage* msg = nullptr;
    while ((msg = gst_bus_pop(bus_)) != nullptr) {
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                // Source URLs may contain credentials or tokens. Preserve only
                // the GLib error domain/code, never the raw message/debug URL.
                GError* error = nullptr;
                gchar* debug = nullptr;
                gst_message_parse_error(msg, &error, &debug);
                if (error) {
                    std::fprintf(stderr, "GStreamer: error domain=%s code=%d\n",
                                 g_quark_to_string(error->domain), error->code);
                    g_error_free(error);
                }
                g_free(debug);
                gst_message_unref(msg);
                stop();
                status_ = "Falha de reproducao; voltou ao menu";
                return true;
            }
            case GST_MESSAGE_EOS:
                std::puts("GStreamer: fim de fluxo recebido (EOS)");
                gst_message_unref(msg);
                stop();
                status_ = "Fim do fluxo";
                return true;
            case GST_MESSAGE_BUFFERING:
                if (!live_) {
                    gint percent = 100;
                    gst_message_parse_buffering(msg, &percent);
                    if (percent < 100) {
                        gst_element_set_state(playbin_, GST_STATE_PAUSED);
                        status_ = "Carregando canal...";
                    } else if (desiredPlaying_) {
                        gst_element_set_state(playbin_, GST_STATE_PLAYING);
                        status_ = "Reproduzindo";
                    }
                    changed = true;
                }
                break;
            case GST_MESSAGE_STATE_CHANGED:
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(playbin_)) {
                    GstState oldState, newState, pending;
                    gst_message_parse_state_changed(msg,&oldState,&newState,&pending);
                    if (newState == GST_STATE_PLAYING && !paused_) {
                        status_ = "Reproduzindo";
                        std::puts("GStreamer: transicao para PLAYING confirmada");
                        changed = true;
                    }
                }
                break;
            default:
                break;
        }
        gst_message_unref(msg);
    }
    return changed;
}

} // namespace raspberry_tv
