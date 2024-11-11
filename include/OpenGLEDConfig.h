#ifndef OPEN_GLED_CONFIG_H
#define OPEN_GLED_CONFIG_H

#include <optional>

#include "yaml-cpp/yaml.h"

class OpenGLEDConfig
{
public:
    static std::optional<OpenGLEDConfig> FromFile(const char* filename);
};

#endif