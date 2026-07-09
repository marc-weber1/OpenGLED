
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

## Building a flashable SD card image

Instead of installing an OS on the Pi, you can build a complete, minimal SD card
image with [Buildroot](https://buildroot.org). From Fedora or WSL2:

```
./build-image.sh
```

The script checks for missing host dependencies (and tells you the exact
`dnf`/`apt` command to fix them), clones Buildroot into `~/opengled-buildroot`,
and builds `opengled-sdcard.img` using all CPU cores. The first build compiles a
cross-toolchain and kernel (1-2 hours); rebuilds are incremental. See
`./build-image.sh` comments for `reconfigure`, `menuconfig` and `clean` actions.

The image boots straight into `open_gled` and is laid out for minimal SD wear:

- **p1 `boot`** (FAT32) - firmware, kernel, `config.txt`
- **p2 `rootfs`** (squashfs) - the OS and app, read-only; safe against power loss
- **p3 `gled-data`** (ext4, label `gled-data`) - the only writable partition,
  holding `/data/config.yaml` and `/data/shaders`. Edit these (from any machine
  that reads ext4, or over the wire) to reconfigure without rebuilding.

The buildroot configuration lives in `buildroot-external/`.

## Rotary encoder (shader switching)

A KY-040 rotary encoder cycles through the shaders in `SHADER_FOLDER`
(clockwise = next, counter-clockwise = previous). Enable it in the config file
with BCM pin numbers (any free GPIOs; avoid SPI 7-11 and I2S 18-21):

```yaml
ROTARY_ENCODER:
  PIN_CLK: 17
  PIN_DT: 27
  GPIOCHIP: /dev/gpiochip0   # optional, this is the default
```

Wire + to 3.3V, GND to ground, CLK/DT to the configured pins (the internal
pull-ups are enabled, no external resistors needed). If the section is absent
or the pins can't be acquired, the app runs normally without it.