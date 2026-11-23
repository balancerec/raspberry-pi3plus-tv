#pragma once
namespace raspberry_tv {
namespace display {
// Full HD internal coordinate system. The current design grid remains 1280x720,
// but every primitive and glyph is rasterized directly in 1920x1080 pixels.
// SDL may downscale the rendered frame when the active HDMI mode is below 1080p.
constexpr int DesignWidth = 1280;
constexpr int DesignHeight = 720;
constexpr int NativeWidth = 1920;
constexpr int NativeHeight = 1080;
constexpr int toNative(int designPx) {
    return (designPx * 3 + (designPx >= 0 ? 1 : -1)) / 2;
}
static_assert(toNative(DesignWidth) == NativeWidth, "1080p width");
static_assert(toNative(DesignHeight) == NativeHeight, "1080p height");
} // namespace display
} // namespace raspberry_tv
