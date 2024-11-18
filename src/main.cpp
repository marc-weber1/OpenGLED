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
#include <math.h>

#include <GLES2/gl2.h>
#include "ALSADevices.hpp"

#include "ws2811.h"
#include "RaspiHeadlessOpenGLContext.h"
#include "Iir.h"

#include "CircularBuffer.h"
#include "OpenGLEDConfig.h"
#include "Shader.h"

#define STRIP_TYPE WS2811_STRIP_GBR // 00 BB GG RR
#define FILTER_ORDER 4

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

float convertS16LEToFloat(const char sample[2]) {
    // Step 1: Combine the two bytes into a signed 16-bit integer
    int16_t intSample = (static_cast<int16_t>(static_cast<uint8_t>(sample[1])) << 8) |
                        static_cast<uint8_t>(sample[0]);

    // Step 2: Normalize to the range [-1.0, 1.0]
    return intSample / 32768.0f; // 32768 is 2^15, the maximum absolute value for int16_t
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

  // Create OpenGL context

  RaspiHeadlessOpenGLContext context = RaspiHeadlessOpenGLContext(config.width, config.height);
  if(!context.Initialize()){
    cerr << "Failed to create a headless OpenGL context.\n";
    return 1;
  }

  context.MakeCurrent();

  // Setup microphone processing

  unique_ptr<ALSACaptureDevice> microphone;
  vector<char> microphone_buffer;

  vector<Iir::Butterworth::BandPass<FILTER_ORDER>> band_filters;
  vector<float> filtered_samples;
  vector<CircularBuffer<unsigned char>> band_pixel_buffers;
  vector<unsigned char> audio_reactive_texture_data;
  GLuint audio_reactive_texture;

  if(config.alsa_input_device != ""){
    microphone = make_unique<ALSACaptureDevice>(config.alsa_input_device, config.sample_rate, 1, config.samples_per_pixel, SND_PCM_FORMAT_S16_LE);
    microphone_buffer.resize(microphone->get_bytes_per_frame() * microphone->get_frames_per_period());
    microphone->open();

    for(int band = 0; band < config.num_bands(); band++){
      band_filters.emplace_back();
      band_filters[band].setup(config.sample_rate, config.center_frequency(band), config.band_width(band));
      band_pixel_buffers.emplace_back(config.pixels_per_band);
    }

    filtered_samples.resize(config.samples_per_pixel);
    audio_reactive_texture_data.resize(config.pixels_per_band * config.num_bands(), 0);

    glGenTextures(1, &audio_reactive_texture);
    glBindTexture(GL_TEXTURE_2D, audio_reactive_texture); // This needs to be called every time if you use any other texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  }

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

  // Do this whole thing on shader initialization
  GLint resolutionLoc = glGetUniformLocation(shaders[current_shader].ID, "resolution");
  glUniform2f(resolutionLoc, (GLfloat) config.width, (GLfloat) config.height);

  // Setup buffer to copy pixel data to LEDs

  vector<char> led_buffer(config.width * config.height * 3);

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

    // Calculate shader audio texture

    if(microphone){
      while(microphone->samples_left_to_read() > config.samples_per_pixel){
        microphone->capture_into_buffer(microphone_buffer.data(), config.samples_per_pixel);

        for(int band = 0; band < config.num_bands(); band++){
          // Filter current buffer
          for(int s = 0; s < config.samples_per_pixel; s++){
            // S16_LE one channel -> float  !! ASSUMES ONE CHANNEL
            filtered_samples[s] = convertS16LEToFloat(microphone_buffer.data() + 2*s);
            // Filter
            filtered_samples[s] = band_filters[band].filter(filtered_samples[s]);
          }

          // Calculate brightness of next pixel from db RMS
          double sum = 0;
          for(int s = 0; s < config.samples_per_pixel; s++){
            sum += filtered_samples[s] * filtered_samples[s];
          }
          double rms = sqrt(sum / config.samples_per_pixel) * 10.0; // Temporary pregain thingy

          //cout << "band " << band << ": " << rms << "\n";

          band_pixel_buffers[band].push_back((unsigned char) (rms * 255.5)); // .5 so it rounds correctly

          // Copy the band data to the audio reactive texture
          band_pixel_buffers[band].peek(audio_reactive_texture_data.data() + band * config.pixels_per_band, config.pixels_per_band);
        }
      }

      // Get audio reactive texture into the GPU
      glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, config.pixels_per_band, config.num_bands(), 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, audio_reactive_texture_data.data());
    }

    // Shader uniforms

    //cout << "Time: " << (GLfloat) (clock() - clock_start)/CLOCKS_PER_SEC << "\n";
    timespec clock_now;
    clock_gettime(CLOCK_MONOTONIC, &clock_now);
    glUniform1f(timeLoc, (GLfloat) seconds_elapsed(clock_start, clock_now));

    // Draw to virtual GBR

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Copy to buffer

    glReadPixels(0, 0, config.width, config.height, GL_RGB, GL_UNSIGNED_BYTE, led_buffer.data());

    // Copy from buffer to LEDs

    for(int y = 0; y < config.height; y++){
      for(int x = 0; x < config.width; x++){
        char* pixel = &led_buffer[(y * config.width + x) * 3];
        
        // Convert a pixel e.g. 0xRRGGBB into 0x00BBGGRR
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
  }

  cout << "\n";

  return ret;

}
