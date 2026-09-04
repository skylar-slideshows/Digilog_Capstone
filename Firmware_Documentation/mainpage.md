# Firmware Developer Documentation

Browse the <a href="files.html">full file list</a>, or here is some of the most useful documentation:

Bucket Controller

- [CONFIG.h](@ref CONFIG.h) - Main configuration file
- [mcp23017.h](@ref mcp23017.h) - Functions to interface with MCP23017 GPIO expander
- [mcp4728.c](@ref mcp4728.c) - Functions to interface with MCP4728 quad DAC
- [i2c_driver.h](@ref i2c_driver.h) - I2C functions
- [led_driver.h](@ref led_driver.h) - Functions to control LED and knob ring displays
- [rotary_encoder.h](@ref rotary_encoder.h) - Functions to find the status and motion of the EC11 rotary encoders
- [control_voltage.h](@ref control_voltage.h) - Control voltage generation engine header
- [console_state_bucket.h](@ref console_state_bucket.h) - Header file for console parameter data saving and reporting code (for the bucket controller)
- LCD DISPLAY DRIVER (need)
- [main.c](@ref Firmware/BucketController/Core/Src/main.c) - Main C file
- [main.h](@ref Firmware/BucketController/Core/Inc/main.h) - Main header


Master Controller
- [main.c](@ref Firmware/MasterController/Core/Src/main.c) - Main C file
- [main.h](@ref Firmware/MasterController/Core/Inc/main.h) - Main header

<br><br><br>
<img src="diagram2.png" width="58%"><br><br><br>
<img src="pins.png" width="72%"><br><br><br>
<img src="pinssr.png" width="72%"><br><br><br>
<img src="pinout.png" width="60%"><br><br><br>
<img src="fullui.png" width="66%"><br><br><br>
<img src="diagram1.png" width="73%"><br><br><br>