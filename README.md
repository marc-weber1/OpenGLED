
## Usage

My setup for this app is on a Raspberry Pi Zero 2W, I am running the LEDs over SPI (GPIO 10) since I need PWM and I2S for a [pin-connected MEMs mic](https://learn.adafruit.com/adafruit-i2s-mems-microphone-breakout/raspberry-pi-wiring-test). Remember to use `arecord -l` to figure out what to put in the config option `ALSA_INPUT_DEVICE`. Here are the relevant parts of `/boot/firmware/config.txt` for an SPI setup:

```
dtparam=i2s=on
dtparam=spi=on

# Lock core frequency to 500 for SPI timing
core_freq=500
core_freq_min=500

# Enable I2S mic
dtoverlay=googlevoicehat-soundcard

# Enable DRM VC4 V3D driver
dtoverlay=vc4-kms-v3d
max_framebuffers=2
```

## Compiling

```
sudo apt install libgles2-mesa-dev libegl1-mesa-dev libgbm-dev libasound2-dev
cmake -Bbuild
cd build
make
```