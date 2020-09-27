# WIFI Spy Tag game

Hey there, this is the code used for the WIFI based hide and seek/tag game using ESP8266 devices.
The idea behind the game is that humans go and hide, while two zombies (or agents) start seeking them out.
once a zombie comes in proximity to a human, they turn into zombies themselves. Eventually this game will
be rebranded to a spy type of game, with more powerups and features to play around with.

This project is currently in basic WIP shape, if you would like to burn and test out this project however,
more info can be found here for how to accomplish this: https://github.com/con-f-use/esp82XX-basic

## Hardware

Coming soon!

## Instructions

### Installation

Espressif IDF toolchain

### Usage

The function `user_init()` in `user/user_main.c` will be run first. 

### Building

First do `make erase`, then `make initdefault #init3v3` to set the flash correctly.
Lastly, use `make burn` to upload to the ESP8266 controller over `/dev/ttyUSB0`
