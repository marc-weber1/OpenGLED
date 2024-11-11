#include <iostream>
#include <vector>
#include <set>
#include <memory>
#include <filesystem>
#include <ctime>
#include <stdint.h>
#include <csignal>
#include <unistd.h>

#include <GLES2/gl2.h>

#include "ws2811.h"
#include "OpenGLEDConfig.h"
#include "RaspiHeadlessOpenGLContext.h"
#include "Shader.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STRIP_TYPE WS2811_STRIP_GBR // 00 BB GG RR

const GLfloat FULLSCREEN_BOX_VEC2[] = {
  -1, -1,
  1, 1,
  -1, 1,

  -1, -1,
  1, -1,
  1, 1,
};

#define STRINGIFY(x) #x
static const char* DEFAULT_VERTEX_SHADER = STRINGIFY(
    attribute vec2 pos; void main() { gl_Position = vec4(pos, 0.0, 1.0); });

static uint8_t running = 1;

static void ctrl_c_handler(int signum){
  running = 0;
}

static void setup_handlers(void){
  signal(SIGINT, ctrl_c_handler);
  signal(SIGTERM, ctrl_c_handler);
}

using namespace std;
namespace fs = std::filesystem;

int main(int argc, char* argv[]){

  // Handle ctrl C
  setup_handlers();

  // Load config file

  optional<OpenGLEDConfig> maybe_config = OpenGLEDConfig::FromFile("../example-config.yaml");
  if(!maybe_config.has_value()){
    cerr << "Failed to parse config file.\n";
    return 1;
  }
  OpenGLEDConfig config = maybe_config.value();

  // Create OpenGL context

  RaspiHeadlessOpenGLContext context = RaspiHeadlessOpenGLContext(config.width, config.height);
  if(!context.Initialize()){
    cerr << "Failed to create a headless OpenGL context.\n";
    return 1;
  }

  context.MakeCurrent();

  // Clear whole screen (front buffer)

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Load shaders from the shader folder

  vector<Shader> shaders;
  
  for (const auto & file : fs::directory_iterator(config.shader_folder)){
    if(file.path().extension() == ".fs"){

      std::string shaderCode;
      std::ifstream shaderFile;

      // ensure ifstream objects can throw exceptions:
      shaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
      try 
      {
        shaderFile.open(file.path());
        std::stringstream shaderStream;
        shaderStream << shaderFile.rdbuf();		
        shaderFile.close();
        shaderCode = shaderStream.str();
      }
      catch(std::ifstream::failure e)
      {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
      }

      shaders.push_back(Shader(DEFAULT_VERTEX_SHADER, shaderCode.c_str()));

    }
  }

  if(shaders.size() == 0){
    cerr << "Could not find any shaders ending with .fs in " << config.shader_folder << "\n";
    return 1;
  }

  int current_shader = 0;
  shaders[current_shader].use();

  // Setup the full screen VAO

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), FULLSCREEN_BOX_VEC2, GL_STATIC_DRAW);

  GLint posLoc = glGetAttribLocation(shaders[current_shader].ID, "pos"); // Also do this for the other shaders
  glEnableVertexAttribArray(posLoc);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                        (void *)0);

  // Get uniforms

  clock_t clock_start = clock();
  GLint timeLoc = glGetUniformLocation(shaders[current_shader].ID, "time"); // Also do this for the other shaders

  // Setup buffer to copy pixel data to LEDs

  unique_ptr<char[]> buffer(new char[config.width * config.height * 3]);

  // Setup LED strip

  ws2811_t ledstring =
  {
    .freq = WS2811_TARGET_FREQ,
    .dmanum = config.dma,
    .channel =
    {
      [0] =
      {
        .gpionum = config.gpio_pin,
        .invert = 0,
        .count = config.width * config.height,
        .strip_type = STRIP_TYPE,
        .brightness = 255,
      },
      [1] =
      {
        .gpionum = 0,
        .invert = 0,
        .count = 0,
        .brightness = 0,
      },
    },
  };

  ws2811_return_t ret;

  /*if((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS){
    cerr << "ws2811_init failed: " << ws2811_get_return_t_str(ret) << "\n";
    return ret;
  }*/

  //while(running){

    // Run a draw call w the shader

    glUniform1f(timeLoc, (GLfloat) (clock() - clock_start)/CLOCKS_PER_SEC);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Copy to buffer

    glReadPixels(0, 0, config.width, config.height, GL_RGB, GL_UNSIGNED_BYTE, buffer.get());

    // Write all pixels to file (temporary)
    int success = stbi_write_png("test.png", config.width, config.height, 3, buffer.get(), config.width * 3);

    // Copy from buffer to LEDs

    /*for(int x = 0; x < config.width; x++){
      ledstring.channel[0].leds[x] = 0x00202000; // cyan
    }

    if((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS){
      cerr << "ws2811_render failed: " << ws2811_get_return_t_str(ret) << "\n";
      break;
    }

    // 15 frames / s
    usleep(1000000 / 15);*/
  //}

  //ws2811_fini(&ledstring);

  cout << "\n";

  return ret;

}
