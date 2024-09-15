# Atari2600_Paddles_to_USB
Arduino Micro code to convert a set of DB9 Paddles to USB

## Introduction
This is a project to convert an a set of DB9 paddles to usb using an Arduino Micro.    The goal is to convert both Atari 2600 and C64 style paddles without making any further modification to the paddles themselves.

Using a charging capacitor approach as decribed on: https://hackaday.io/project/170908-control-freak   (see link also for wiring diagrams), it works by charging a capacitor then timing how long it takes to discharge.
The time taken to discharge is controlled by the pot (or dial) on the paddle.  It then converts this value into a gamepad analog stick value to mimic a gamepad analog stick values being moved left and right.

## Current Status
It is experimental code and unfortunately suffers from jitter in reading pot values.    Its stable enough to use the arduino's  Serial Monitor and Window's setup usb game controller to test its function.

Download: https://github.com/dekkit/Atari2600_Paddles_to_USB/blob/main/Atari2600_Paddles_to_USB.ino

## Further Information
It has two modes:
- GAMEPAD
- MOUSE

Suggest to use the gamepad mode as paddle controllers make terrible mouse altenatives 

I've documented my understanding within the .ino


## Alternatives
If you want to try an alternative that may work better for your needs, see the following link... (note: not my project)

https://github.com/MiSTer-devel/Retro-Controllers-USB-MiSTer/tree/master/PaddleTwoControllersUSB


