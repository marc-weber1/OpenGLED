#include "OpenGLEDConfig.h"

#include <stdexcept>

std::optional<OpenGLEDConfig> OpenGLEDConfig::FromFile(const char* filename)
{
    OpenGLEDConfig return_config;
    YAML::Node config = YAML::LoadFile("../example-config.yaml");

    if(config["LED_SETTINGS"]){
        return_config.gpio_pin = config["LED_SETTINGS"]["GPIO_PIN"].as<int>();
        return_config.width = config["LED_SETTINGS"]["WIDTH"].as<int>();
        return_config.height = config["LED_SETTINGS"]["HEIGHT"].as<int>();

        if(config["LED_SETTINGS"]["DMA"])
            return_config.dma = config["LED_SETTINGS"]["DMA"].as<int>();
        if(config["LED_SETTINGS"]["BRIGHTNESS"])
            return_config.brightness = config["LED_SETTINGS"]["BRIGHTNESS"].as<uint8_t>();
        if(config["LED_SETTINGS"]["GAMMA_CORRECTION"])
            return_config.gamma_correction = config["LED_SETTINGS"]["GAMMA_CORRECTION"].as<float>();
    }

    if(config["AUDIO_SETTINGS"]){
        return_config.alsa_input_device = config["AUDIO_SETTINGS"]["ALSA_INPUT_DEVICE"].as<std::string>();

        if(config["LED_SETTINGS"]["FREQUENCY_BANDS"]){
            uint num_bands = config["LED_SETTINGS"]["FREQUENCY_BANDS"].as<uint>();
            if(config["LED_SETTINGS"]["BAND_CUTOFF_FREQUENCIES"].size() != num_bands + 1){
                throw std::runtime_error("BAND_CUTOFF_FREQUENCIES needs to give one more cutoff frequency than the number of bands.");
            }

            return_config.frequency_bands.clear();
            for(YAML::Node cutoff_freq : config["LED_SETTINGS"]["BAND_CUTOFF_FREQUENCIES"]){
                return_config.frequency_bands.push_back(cutoff_freq.as<float>());
            }
        }

        if(config["LED_SETTINGS"]["SAMPLE_RATE"])
            return_config.sample_rate = config["LED_SETTINGS"]["SAMPLE_RATE"].as<int>();

        if(config["LED_SETTINGS"]["SAMPLES_PER_PIXEL"])
            return_config.samples_per_pixel = config["LED_SETTINGS"]["SAMPLES_PER_PIXEL"].as<int>();

        if(config["LED_SETTINGS"]["PIXELS_PER_IMAGE"])
            return_config.pixels_per_image = config["LED_SETTINGS"]["PIXELS_PER_IMAGE"].as<int>();
    }

    return_config.shader_folder = config["SHADER_FOLDER"].as<std::string>();

    return return_config;
}