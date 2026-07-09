#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <string>

// Reads a KY-040 style quadrature rotary encoder through the Linux GPIO
// character device (/dev/gpiochipN), no external library needed.
class RotaryEncoder
{
public:
    RotaryEncoder(int pin_clk, int pin_dt, const std::string &gpiochip_path = "/dev/gpiochip0");
    ~RotaryEncoder();

    // Requests the GPIO lines with pull-ups and edge detection.
    // Returns false (and prints why) if the lines can't be acquired.
    bool Initialize();

    // Non-blocking. Drains any queued edge events and returns the net number
    // of detents turned since the last call (positive = clockwise).
    int ReadDelta();

private:
    std::string gpiochip_path;
    int pin_clk, pin_dt;
    int line_fd = -1;
    unsigned char levels = 0; // bit 1 = CLK level, bit 0 = DT level
    int quarter_steps = 0;    // a KY-040 detent is one full quadrature cycle (4 steps)
};

#endif
