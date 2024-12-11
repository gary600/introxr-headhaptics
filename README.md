# `introxr-headhaptics`: VRChat Head Haptics
18-453 Intro to XR Final Project

## VRChat Setup
Need to use an avatar that's adapted for this, for example,
[this version](https://github.com/gary600/vrcfox-haptics)
of the Simple Fox public avatar (CC-BY-NC).

Additionally, need to setup VRChat for OSC: if `haptics-server` is running on the same
computer just enable the OSC switch in the quick menu, otherwise you need to additionally
pass the `--osc=9000:ip.of.haptics.server:9001` command line argument.

## `haptics-server`
Python script that receives OSC from VRChat and converts to my serial format

Libraries used:
- [PySerial](https://github.com/pyserial/pyserial) (BSD license)
- [python-osc](https://github.com/attwad/python-osc) (Unlicense)

## `haptics-device`
Raspberry Pi Pico firmware, receives my serial data and triggers haptics actuators

Libraries used:
- [Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) (BSD license)
