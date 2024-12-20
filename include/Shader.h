#ifndef SHADER_H
#define SHADER_H
  
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <GLES2/gl2.h>

class Shader
{
public:
    // the program ID
    GLuint ID;
  
    // constructor reads and builds the shader
    Shader(const char* vertexCode, const char* fragmentCode);
    ~Shader();
    // use/activate the shader
    void use();
    // utility uniform functions
    void setBool(const std::string &name, bool value) const;  
    void setInt(const std::string &name, int value) const;   
    void setFloat(const std::string &name, float value) const;
};
  
#endif