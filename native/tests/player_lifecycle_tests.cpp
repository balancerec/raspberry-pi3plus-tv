#include <gst/gst.h>
#include <cstdio>
#include <cstdlib>

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "GStreamer lifecycle test failed: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc, char** argv) {
    gst_init(&argc, &argv);
    // Turn any GStreamer/GLib critical in this test into a failure.
    g_log_set_always_fatal(static_cast<GLogLevelFlags>(G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL));

    for (int cycle = 0; cycle < 24; ++cycle) {
        GstElement* playbin = gst_element_factory_make("playbin", nullptr);
        require(playbin != nullptr, "playbin factory unavailable");
        gst_object_ref_sink(GST_OBJECT(playbin));

        GstElement* sink = gst_element_factory_make("fakesink", nullptr);
        require(sink != nullptr, "fakesink factory unavailable");
        gst_object_ref_sink(GST_OBJECT(sink));

        // Playbin must keep a valid reference to sink after our unref.
        g_object_set(G_OBJECT(playbin), "video-sink", sink, nullptr);
        gst_object_unref(sink);

        GstBus* bus = gst_element_get_bus(playbin);
        require(bus != nullptr, "playbin bus unavailable");

        require(gst_element_set_state(playbin, GST_STATE_NULL) != GST_STATE_CHANGE_FAILURE,
                "transition to NULL failed");
        gst_element_get_state(playbin, nullptr, nullptr, GST_SECOND);

        gst_object_unref(bus);
        gst_object_unref(playbin);
    }
    std::puts("GStreamer lifetime: 24 create/property/NULL/release cycles passed");
    return 0;
}
