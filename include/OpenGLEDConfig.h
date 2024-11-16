#ifndef OPEN_GLED_CONFIG_H
#define OPEN_GLED_CONFIG_H

#include <optional>
#include <string>
#include <vector>

#include "yaml-cpp/yaml.h"

class OpenGLEDConfig
{
public:
    // LED settings
    int dma = 10, gpio_pin = 18, width = 0, height = 0;
    float gamma_correction = 1.0;
    uint8_t brightness = 32;

    std::string shader_folder;

    // Audio settings
    std::string alsa_input_device;
    std::vector<float> frequency_bands;
    int channels = 1, sample_rate = 44100, samples_per_pixel = 1024, pixels_per_band = 144;

    int num_bands(){ return frequency_bands.size() - 1; }
    float center_frequency(int band){ return frequency_bands[band] + (frequency_bands[band+1] - frequency_bands[band]) / 2.f; }
    float band_width(int band){ return frequency_bands[band+1] - frequency_bands[band]; }

    static std::optional<OpenGLEDConfig> FromFile(const char* filename);
};

#endif