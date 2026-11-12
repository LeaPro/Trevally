/*
 * version.h - Firmware Version Number definition (same scheme as the DLS amp's version.h).
 *
 * MAX allowed number for each field is "99"
 * MAJOR_VER    Change when reverse compatibility is lost or major distiction needs to be made
 * MINOR_VER    Change when major features changed/added/removed
 * RELEASE_VER  Change when released to mfg/field
 * BUILD_VER    Change when built, reset when any other number changes
 * RC_NUM       Release candidate, e.g. "RC1"; "" for none
 */
#pragma once

#define MAJOR_VER    0
#define MINOR_VER    0
#define RELEASE_VER  0
#define BUILD_VER    0
#define RC_NUM    "RC1"

// 0.0.0.0-RC1 initial version number
