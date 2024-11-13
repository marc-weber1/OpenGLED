#include <iostream>
#include <vector>
#include <set>
#include <memory>
#include <filesystem>
#include <regex>
#include <ctime>
#include <stdint.h>
#include <csignal>
#include <unistd.h>

#include <GLES2/gl2.h>
#include "ALSADevices.hpp"

#include "ws2811.h"
#include "OpenGLEDConfig.h"
#include "RaspiHeadlessOpenGLContext.h"
#include "Shader.h"

#define STRIP_TYPE WS2811_STRIP_GBR // 00 BB GG RR

const int AUDIO_BUFFER_SIZE = 256;
const int AUDIO_SAMPLE_RATE = 44100;

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

std::string read_file(fs::path filepath){
  std::string shaderCode;
  std::ifstream shaderFile;

  // ensure ifstream objects can throw exceptions:
  shaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
  try 
  {
    shaderFile.open(filepath);
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();		
    shaderFile.close();
    shaderCode = shaderStream.str();
  }
  catch(std::ifstream::failure e)
  {
    std::cerr << "Failed to read file " << filepath << std::endl;
  }

  return shaderCode;
}

float seconds_elapsed(timespec start, timespec end){
  long ns_elapsed = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
  return (float) ns_elapsed / 1000000000.f;
}

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

  // Setup microphone processing

  unique_ptr<ALSACaptureDevice> microphone;
  char* microphone_buffer;

  if(config.alsa_input_device != ""){
    microphone = make_unique<ALSACaptureDevice>(config.alsa_input_device, AUDIO_SAMPLE_RATE, 1, AUDIO_BUFFER_SIZE, SND_PCM_FORMAT_S16_LE);
    microphone_buffer = microphone->allocate_buffer();
    microphone->open();
  }

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

      string shaderCode = read_file(file.path());

      /*if(config.gamma_correction != 1.0){
        // Hotpatch the fragment shader with gamma correction:
        //  e.g. gl_FragColor = pow(gl_FragColor, vec4(1.7, 1.7, 1.7, 1));
        regex fragment_main_function_regex("void\\s+main\\s*\\(\\s*\\)\\s*\\{([^\\}]*)\\}");
        shaderCode = regex_replace(shaderCode, fragment_main_function_regex, "void main(){$1\n\tgl_FragColor = pow(gl_FragColor, vec4("
          + to_string(config.gamma_correction)
          + "," + to_string(config.gamma_correction)
          + "," + to_string(config.gamma_correction)
          + ",1));\n}");

        cout << shaderCode << "\n"; // debug
      }*/

      shaders.emplace_back(DEFAULT_VERTEX_SHADER, shaderCode.c_str()); // Have to be careful with copies since the shader destroys on deconstruct
      
    }
  }

  if(shaders.size() == 0){
    cerr << "Could not find any shaders ending with .fs in " << config.shader_folder << "\n";
    return 1;
  }

  int current_shader = 0;
  shaders[current_shader].use();

  // Setup the full screen VBO

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), FULLSCREEN_BOX_VEC2, GL_STATIC_DRAW);

  GLint posLoc = glGetAttribLocation(shaders[current_shader].ID, "pos"); // Also do this for the other shaders
  glEnableVertexAttribArray(posLoc);
  glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);

  // Get uniforms

  timespec clock_start;
  clock_gettime(CLOCK_MONOTONIC, &clock_start);
  GLint timeLoc = glGetUniformLocation(shaders[current_shader].ID, "time"); // Also do this for the other shaders

  // Setup buffer to copy pixel data to LEDs

  vector<char> buffer(config.width * config.height * 3);

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
        .brightness = config.brightness,
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

  if((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS){
    cerr << "ws2811_init failed: " << ws2811_get_return_t_str(ret) << "\n";
    return ret;
  }

  while(running){

    // Run a draw call w the shader

    /*if(microphone){ // This uses the SAME PWM MODULE AS PIN 12 I'M GONNA KMS
      microphone->capture_into_buffer(microphone_buffer, AUDIO_BUFFER_SIZE);
    }*/

    //cout << "Time: " << (GLfloat) (clock() - clock_start)/CLOCKS_PER_SEC << "\n";
    timespec clock_now;
    clock_gettime(CLOCK_MONOTONIC, &clock_now);
    glUniform1f(timeLoc, (GLfloat) seconds_elapsed(clock_start, clock_now));

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Copy to buffer

    glReadPixels(0, 0, config.width, config.height, GL_RGB, GL_UNSIGNED_BYTE, buffer.data());

    // Copy from buffer to LEDs

    for(int y = 0; y < config.height; y++){
      for(int x = 0; x < config.width; x++){
        char* pixel = &buffer[(y * config.width + x) * 3];
        
        ledstring.channel[0].leds[y * config.width + x] = (pixel[2] << 16) | (pixel[1] << 8) | pixel[0];
      }
    }

    if((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS){
      cerr << "ws2811_render failed: " << ws2811_get_return_t_str(ret) << "\n";
      break;
    }

    // 15 frames / s  (NOT how frame timing works ...)
    //usleep(1000000 / 15);
  }

  ws2811_fini(&ledstring);

  if(microphone){
    microphone->close();
    free(microphone_buffer);
  }

  cout << "\n";

  return ret;

}
