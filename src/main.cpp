#include <iostream>
#include <vector>
#include <stdint.h>
#include <csignal>
#include <unistd.h>

#include "ws2811.h"
#include "OpenGLEDConfig.h"
#include "RaspiHeadlessOpenGLContext.h"

#define STRIP_TYPE WS2811_STRIP_GBR // 00 BB GG RR

static uint8_t running = 1;

static void ctrl_c_handler(int signum){
  running = 0;
}

static void setup_handlers(void){
  signal(SIGINT, ctrl_c_handler);
  signal(SIGTERM, ctrl_c_handler);
}

using namespace std;

int main(int argc, char* argv[]){

  setup_handlers();

  optional<OpenGLEDConfig> maybe_config = OpenGLEDConfig::FromFile("../example-config.yaml");
  if(!maybe_config.has_value()){
    cerr << "Failed to parse config file.\n";
    return 1;
  }
  OpenGLEDConfig config = maybe_config.value();

  RaspiHeadlessOpenGLContext context = RaspiHeadlessOpenGLContext(config.width, config.height);
  if(!context.Initialize()){
    cerr << "Failed to create a headless OpenGL context.\n";
    return 1;
  }

  context.MakeCurrent();

  ws2811_return_t ret;

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

  if((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS){
    cerr << "ws2811_init failed: " << ws2811_get_return_t_str(ret) << "\n";
    return ret;
  }

  while(running){
    for(int x = 0; x < config.width; x++){
      ledstring.channel[0].leds[x] = 0x00202000; // cyan
    }

    if((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS){
      cerr << "ws2811_render failed: " << ws2811_get_return_t_str(ret) << "\n";
      break;
    }

    // 15 frames / s
    usleep(1000000 / 15);
  }

  ws2811_fini(&ledstring);

  cout << "\n";

  return ret;

}
