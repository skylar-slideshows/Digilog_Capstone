# LVGL (subset) for the Digilog master controller

From the LVGL download in this project's history (`lvgl.zip`, the `master` branch: `lv_version.h`
says 10.0.0-dev, `library.json` says 9.6.0). MIT licence: see `LICENCE.txt`.

Only what the master uses is here, to keep the project small:

- `lvgl.h`, `lvgl_private.h` and the whole `include/` tree (the public headers)
- in `src/`: the core, display, indev, tick, timers, the software renderer (`draw/`, `draw/sw/`,
  `draw/sw/blend/`), image decoding, fonts (`lv_font.c`, `fmt_txt/`, Montserrat 14 and 28),
  `misc/` (with `cache/`), the built-in memory and string functions, `osal` (no OS), the
  theme core, the file-system core, logging, and the label widget, plus the private headers
  those include

The configuration is `Core/Inc/lv_conf.h`. CMake compiles every `.c` file under `src/`, so to use
another widget or font: switch it on in `lv_conf.h` and copy its `.c` file(s) from the full LVGL
download into the same place under `src/` (e.g. `src/widgets/bar/lv_bar.c` for `LV_USE_BAR`).
If the build then can't find a header, copy that too.
