#include "StartupDiagnostics.h"

#include <filesystem>
#include <iostream>
#include <string>

#include <unistd.h>
#include <alsa/asoundlib.h>

namespace fs = std::filesystem;
using std::cout;
using std::to_string;

static void ok(const std::string &message)  { cout << "[ OK ] " << message << "\n"; }
static void fail(const std::string &message){ cout << "[FAIL] " << message << "\n"; }
static void note(const std::string &message){ cout << "[    ] " << message << "\n"; }

// Prints every ALSA capture device like `arecord -l` would; returns whether any exist
static bool list_alsa_capture_devices()
{
    bool found_any = false;
    int card = -1;

    while(snd_card_next(&card) >= 0 && card >= 0){
        char ctl_name[32];
        snprintf(ctl_name, sizeof(ctl_name), "hw:%d", card);

        snd_ctl_t *ctl;
        if(snd_ctl_open(&ctl, ctl_name, 0) < 0)
            continue;

        snd_ctl_card_info_t *card_info;
        snd_ctl_card_info_alloca(&card_info);
        if(snd_ctl_card_info(ctl, card_info) < 0){
            snd_ctl_close(ctl);
            continue;
        }

        int dev = -1;
        while(snd_ctl_pcm_next_device(ctl, &dev) >= 0 && dev >= 0){
            snd_pcm_info_t *pcm_info;
            snd_pcm_info_alloca(&pcm_info);
            snd_pcm_info_set_device(pcm_info, dev);
            snd_pcm_info_set_subdevice(pcm_info, 0);
            snd_pcm_info_set_stream(pcm_info, SND_PCM_STREAM_CAPTURE);
            if(snd_ctl_pcm_info(ctl, pcm_info) < 0)
                continue; // this device has no capture stream

            found_any = true;
            cout << "         hw:" << card << "," << dev
                 << "  " << snd_ctl_card_info_get_name(card_info)
                 << " - " << snd_pcm_info_get_name(pcm_info) << "\n";
        }
        snd_ctl_close(ctl);
    }

    return found_any;
}

DiagnosticsResult run_startup_diagnostics(const OpenGLEDConfig &config)
{
    DiagnosticsResult result;

    cout << "=== OpenGLED startup diagnostics ===\n";

    if(geteuid() == 0)
        ok("Running as root");
    else
        fail("Not running as root -> the LED driver needs root for /dev/spidev0.0 or /dev/mem");

    // GPU / DRM

    if(fs::exists("/dev/dri/card0"))
        ok("GPU: /dev/dri/card0 exists");
    else
        fail("GPU: /dev/dri/card0 missing -> vc4 driver not loaded. Check 'dtoverlay=vc4-kms-v3d' in config.txt and 'dmesg | grep vc4'");

    if(fs::exists("/dev/dri/card1"))
        note("GPU: /dev/dri/card1 also exists -> if the GL context fails, card0 may be the wrong device (app uses card0)");

    // LED output path

    if(config.gpio_pin == 10){
        if(fs::exists("/dev/spidev0.0"))
            ok("LEDs: GPIO 10 (SPI), /dev/spidev0.0 exists");
        else
            fail("LEDs: GPIO 10 needs SPI but /dev/spidev0.0 is missing -> check 'dtparam=spi=on' in config.txt and that the spidev module is loaded");
    } else {
        note("LEDs: GPIO " + to_string(config.gpio_pin) + " uses PWM/PCM via DMA (no device node to check; requires root)");
    }

    // Audio

    if(config.alsa_input_device == ""){
        note("Audio: no ALSA_INPUT_DEVICE configured, audio reactivity disabled");
    } else {
        cout << "[    ] ALSA capture devices found:\n";
        if(!list_alsa_capture_devices())
            fail("Audio: no ALSA capture devices at all -> I2S mic driver not loaded. Check 'dtoverlay=googlevoicehat-soundcard' in config.txt and 'dmesg | grep -i voicehat'");

        snd_pcm_t *pcm;
        int rc = snd_pcm_open(&pcm, config.alsa_input_device.c_str(), SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
        if(rc >= 0){
            snd_pcm_close(pcm);
            ok("Audio: configured device '" + config.alsa_input_device + "' opened for capture");
            result.audio_device_ok = true;
        } else {
            fail("Audio: cannot open configured device '" + config.alsa_input_device + "': "
                 + snd_strerror(rc) + " -> compare with the device list above (e.g. plughw:0)");
        }
    }

    // Shaders

    std::error_code ec;
    if(!fs::is_directory(config.shader_folder, ec)){
        fail("Shaders: folder '" + config.shader_folder + "' does not exist -> is the data partition mounted? (mount | grep data)");
    } else {
        int count = 0;
        for(const auto &file : fs::directory_iterator(config.shader_folder, ec))
            if(file.path().extension() == ".fs")
                count++;

        if(count > 0)
            ok("Shaders: found " + to_string(count) + " .fs file(s) in " + config.shader_folder);
        else
            fail("Shaders: no .fs files in '" + config.shader_folder + "'");
    }

    // Rotary encoder

    if(config.encoder_pin_clk >= 0 && config.encoder_pin_dt >= 0){
        if(fs::exists(config.encoder_gpiochip))
            ok("Encoder: " + config.encoder_gpiochip + " exists (CLK=" + to_string(config.encoder_pin_clk)
               + ", DT=" + to_string(config.encoder_pin_dt) + ")");
        else
            fail("Encoder: " + config.encoder_gpiochip + " missing -> GPIO character device not available in this kernel");
    }

    cout << "====================================\n";

    return result;
}
