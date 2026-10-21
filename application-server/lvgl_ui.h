#pragma once
#include <cstdint>
#include <boost/asio/io_context.hpp>

// Initializes LVGL, creates a simple "Hello LEA" test screen, and schedules
// its timer/task loop onto the given io_context instead of a dedicated
// thread, so LVGL and all other application I/O share the single io_context
// thread. Must be called once, before io_context.run(). Uses the SDL2
// backend on a host/dev-machine build, or the DRM+evdev backend on a target
// build (see makefile).
void lvglUiInit(boost::asio::io_context &io_context);
