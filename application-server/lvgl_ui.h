#pragma once

// Starts a background thread that initializes LVGL, creates a simple
// "Hello LEA" test screen, and runs the LVGL timer/task loop for the
// lifetime of the process. Uses the SDL2 backend on a host/dev-machine
// build, or the DRM+evdev backend on a target build (see makefile).
void lvglThreadStart();
