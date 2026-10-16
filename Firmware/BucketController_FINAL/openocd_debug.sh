#!/bin/sh

openocd \
  -f interface/stlink.cfg \
  -f target/stm32g4x.cfg
