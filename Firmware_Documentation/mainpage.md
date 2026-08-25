# Firmware Developer Documentation

Browse the <a href="files.html">full file list</a>, or here is some of the most useful documentation:

Bucket Controller

- [mcp23017_driver.h](@ref mcp23017_driver.h) - Functions to interface with MCP23017 GPIO expander
- [mcp4728_driver.h](@ref mcp4728_driver.h) - Functions to interface with MCP4728 quad DAC
- [i2c_driver.h](@ref i2c_driver.h) - I2C functions
- [led_driver.h](@ref led_driver.h) - Functions to control LED and knob ring displays
- [control_voltage.h](@ref control_voltage.h) - Control voltage generation engine header
- [console_state_bucket.h](@ref console_state_bucket.h) - Header file for console parameter data saving and reporting code (for the bucket controller)
- LCD DISPLAY DRIVER (need)
- [main.c](@ref Firmware/BucketController/Core/Src/main.c) - Main C file
- [main.h](@ref Firmware/BucketController/Core/Inc/main.h) - Main header


Master Controller
- [main.c](@ref Firmware/MasterController/Core/Src/main.c) - Main C file
- [main.h](@ref Firmware/MasterController/Core/Inc/main.h) - Main header

<br><br><br>
<img src="fullui.png" width="66%"><br><br><br>
<img src="diagram2.png" width="65%"><br><br><br>
<img src="pins.png" width="60%"><br><br><br>
<img src="pinout.png" width="60%"><br><br><br>
<img src="diagram1.png" width="73%"><br><br><br>