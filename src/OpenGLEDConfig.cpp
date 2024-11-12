#include "OpenGLEDConfig.h"

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

    return_config.shader_folder = config["SHADER_FOLDER"].as<std::string>();
    if(config["ALSA_INPUT_DEVICE"])
        return_config.alsa_input_device = config["ALSA_INPUT_DEVICE"].as<std::string>();

    return return_config;
}