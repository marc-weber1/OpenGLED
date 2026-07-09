#ifndef STARTUP_DIAGNOSTICS_H
#define STARTUP_DIAGNOSTICS_H

#include "OpenGLEDConfig.h"

struct DiagnosticsResult {
    bool audio_device_ok = false;
};

// Prints a [ OK ]/[FAIL] verdict for every known failure mode (GPU node, SPI
// device, ALSA capture devices, shader folder, rotary encoder GPIO) so boot
// problems can be diagnosed from the console alone, without a keyboard.
DiagnosticsResult run_startup_diagnostics(const OpenGLEDConfig &config);

#endif
