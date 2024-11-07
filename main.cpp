#include <iostream>
#include <stdint.h>
#include <csignal>
#include <unistd.h>

#include "ws2811.h"

#define DMA 10
#define GPIO_PIN 12
#define LED_COUNT 144
#define STRIP_TYPE WS2811_STRIP_GBR // 00 GG BB RR

ws2811_t ledstring =
{
  .freq = WS2811_TARGET_FREQ,
  .dmanum = DMA,
  .channel =
  {
    [0] =
    {
      .gpionum = GPIO_PIN,
      .invert = 0,
      .count = LED_COUNT,
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

  ws2811_return_t ret;

  setup_handlers();

  if((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS){
    cerr << "ws2811_init failed: " << ws2811_get_return_t_str(ret) << "\n";
    return ret;
  }

  while(running){
    for(int x = 0; x < LED_COUNT; x++){
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
