#include "RotaryEncoder.h"

#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

RotaryEncoder::RotaryEncoder(int pin_clk, int pin_dt, const std::string &gpiochip_path)
    : gpiochip_path(gpiochip_path), pin_clk(pin_clk), pin_dt(pin_dt) {}

RotaryEncoder::~RotaryEncoder()
{
    if(line_fd >= 0)
        close(line_fd);
}

bool RotaryEncoder::Initialize()
{
    int chip_fd = open(gpiochip_path.c_str(), O_RDONLY | O_CLOEXEC);
    if(chip_fd < 0){
        std::cerr << "RotaryEncoder: failed to open " << gpiochip_path << ": " << strerror(errno) << "\n";
        return false;
    }

    struct gpio_v2_line_request req;
    memset(&req, 0, sizeof(req));
    req.offsets[0] = pin_clk;
    req.offsets[1] = pin_dt;
    req.num_lines = 2;
    strncpy(req.consumer, "open_gled", sizeof(req.consumer) - 1);
    req.config.flags = GPIO_V2_LINE_FLAG_INPUT
                     | GPIO_V2_LINE_FLAG_BIAS_PULL_UP
                     | GPIO_V2_LINE_FLAG_EDGE_RISING
                     | GPIO_V2_LINE_FLAG_EDGE_FALLING;
    // Light debounce against contact chatter; a hand-turned detent is far slower than 1ms
    req.config.num_attrs = 1;
    req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE;
    req.config.attrs[0].attr.debounce_period_us = 1000;
    req.config.attrs[0].mask = 0b11;

    int ret = ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req);
    close(chip_fd);
    if(ret < 0 || req.fd < 0){
        std::cerr << "RotaryEncoder: failed to request GPIO " << pin_clk << "/" << pin_dt
                  << " on " << gpiochip_path << ": " << strerror(errno) << "\n";
        return false;
    }
    line_fd = req.fd;

    struct gpio_v2_line_values vals;
    vals.mask = 0b11;
    vals.bits = 0;
    if(ioctl(line_fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &vals) >= 0){
        // request order: bit 0 of vals = CLK, bit 1 of vals = DT
        levels = (unsigned char) (((vals.bits & 1) ? 0b10 : 0) | ((vals.bits & 2) ? 0b01 : 0));
    }

    return true;
}

int RotaryEncoder::ReadDelta()
{
    if(line_fd < 0)
        return 0;

    // Quadrature state machine, indexed by (previous state << 2) | new state
    static const signed char TRANSITIONS[16] = {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0,
    };

    int detents = 0;
    struct pollfd pfd = { line_fd, POLLIN, 0 };

    while(poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)){
        struct gpio_v2_line_event events[16];
        ssize_t bytes = read(line_fd, events, sizeof(events));
        if(bytes <= 0)
            break;

        for(size_t i = 0; i < bytes / sizeof(events[0]); i++){
            int level = (events[i].id == GPIO_V2_LINE_EVENT_RISING_EDGE) ? 1 : 0;
            unsigned char bit = (events[i].offset == (unsigned) pin_clk) ? 0b10 : 0b01;
            unsigned char new_levels = level ? (levels | bit) : (levels & (unsigned char) ~bit);

            quarter_steps += TRANSITIONS[(levels << 2) | new_levels];
            levels = new_levels;

            if(quarter_steps >= 4){
                detents++;
                quarter_steps = 0;
            } else if(quarter_steps <= -4){
                detents--;
                quarter_steps = 0;
            }
        }
    }

    return detents;
}
