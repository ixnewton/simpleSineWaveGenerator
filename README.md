# Simple Sine Wave Generator (Linux GTK + PulseAudio)

This is a simple sine wave generator project for Linux using GTK3 for the GUI and PulseAudio for audio output. It provides real-time control over volume, frequency, and phase of a sine wave.

## Features

- **Volume Control**: Adjustable from -96 dB to +6 dB
- **Frequency Control**: Adjustable from 10 Hz to 22 kHz
- **Phase Control**: Adjustable from 0 to 1 (0 to 2π radians)
- **Mute Button**: Instant audio mute/unmute
- **Real-time Audio**: Continuous sine wave generation via PulseAudio

## Building

### Dependencies

- GTK3 development libraries
- PulseAudio development libraries
- C++11 compatible compiler (g++)

On Arch Linux:
```bash
sudo pacman -S gtk3 pulseaudio
```

On Ubuntu/Debian:
```bash
sudo apt-get install libgtk-3-dev libpulse-dev
```

### Build Instructions

```bash
make -f Makefile.gtk
```

## Running

```bash
./gtk_sine_generator
```

## Usage

1. Launch the application
2. Adjust the volume slider to set output level
3. Adjust the frequency slider to change pitch
4. Adjust the phase slider to shift the waveform
5. Use the mute button to silence output

## Technical Details

- **GUI Framework**: GTK3
- **Audio Backend**: PulseAudio (Simple API)
- **Sample Rate**: 44.1 kHz
- **Channels**: Stereo
- **Sample Format**: 32-bit float
- **Threading**: Separate audio thread for continuous generation

