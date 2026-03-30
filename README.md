# esp-ubus

This project provides an ubus-based control interface for controlling ESP devices.

## Features
  + Detects all connected ESP devices
  + Uses serial for sending commands
  + Creates a seperate ubus service for each connected ESP device.
  + ESP device control via UBUS

## Prerequisites
  + ESP Microcontroller(s) with special firmware (https://github.com/janenasl/esp_control_over_serial)
  + Packages `libserialport`, `libubus`, `libubox`, `libblobmsg-json`
