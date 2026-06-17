# Simple Wave Generator (Linux GTK + PulseAudio)

This is a simple wave generator project for Linux using GTK3 for the GUI and PulseAudio for audio output. It provides real-time control over volume, frequency, and phase of sine and square waves.

## Features

- **Waveform Selection**: Choose between sine and square wave output
- **Volume Control**: Adjustable from -96 dB to +6 dB (default -20 dB)
- **Frequency Control**: Adjustable from 10 Hz to 22 kHz
- **Phase Control**: Adjustable from 0 to 1 (0 to 2π radians)
- **Play/Mute Buttons**: Toggle continuous sine or square wave output on/off
- **Waveform Display**: Real-time visualization of the selected waveform
- **Logarithmic Frequency Sweep**: Generate frequency sweeps with logarithmic spacing
- **Sweep Direction**: Sweep up (Start → End) or sweep down (End → Start)
- **Sweep Duration**: Adjustable from 0.1 to 60 seconds (default 5 seconds)
- **Real-time Audio**: Continuous wave generation via PulseAudio/PipeWire.

## Installation

### Arch Linux (AUR)

Install from the Arch User Repository using an AUR helper:

```bash
yay -S simplesinewavegenerator-git
# or
paru -S simplesinewavegenerator-git
```

Or manually build from the AUR:

```bash
git clone https://aur.archlinux.org/simplesinewavegenerator-git.git
cd simplesinewavegenerator-git
makepkg -si
```

### Building from Source

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

After installation from AUR:

```bash
simpleSineWaveGenerator
```

When building from source:

```bash
./simpleSineWaveGenerator
```

## Usage

1. Launch the application
2. Select waveform type (Sine or Square)
3. Click "Play" for the selected waveform to enable continuous output
4. Adjust the volume slider to set output level
5. Adjust the frequency slider to change pitch
6. Adjust the phase slider to shift the waveform
7. Use "Mute" to silence continuous output
8. View the real-time waveform display in the bottom panel

### Frequency Sweep

1. Set the Start frequency (Hz)
2. Set the End frequency (Hz)
3. Set the sweep duration (seconds)
4. Click "Sweep ->" to sweep from Start to End frequency (ascending)
5. Click "Sweep <-" to sweep from End to Start frequency (descending)
6. The sweep uses logarithmic frequency spacing for smooth transitions
7. Sweep plays independently of the Play/Mute button and stops automatically when complete
8. Generated sweeps are saved to `sweep_up.wav` or `sweep_down.wav` for analysis

## Technical Details

- **GUI Framework**: GTK3
- **Audio Backend**: PulseAudio (Simple API)
- **Sample Rate**: 44.1 kHz
- **Channels**: Stereo
- **Sample Format**: 32-bit float
- **Threading**: Separate audio thread for continuous generation
- **Waveform Types**: Sine and square wave generation
- **Waveform Display**: Real-time visualization using Cairo graphics
- **Sweep Generation**: Pre-generated buffer with logarithmic frequency spacing and phase accumulation
- **Sweep Storage**: Raw PCM buffers converted to WAV files for debug/analysis

