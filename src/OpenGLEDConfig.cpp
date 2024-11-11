#include "OpenGLEDConfig.h"

std::optional<OpenGLEDConfig> OpenGLEDConfig::FromFile(const char* filename)
{
    OpenGLEDConfig return_config;
    YAML::Node config = YAML::LoadFile("../example-config.yaml");
    return return_config;
}