# music-box-bluepill

Adaptation of [music-box-wb55-rtos](https://github.com/Sterben624/music-box-wb55-rtos)
for the Blue Pill board (STM32F103C8T6).

The original version was built on a Nucleo-WB55, which was mainly chosen
because it was the board available at hand for debugging during early
development, rather than for any specific feature of the WB55 the project
actually relied on. Since the project didn't need anything WB55-specific,
this final iteration moves the whole design over to a Blue Pill clone,
which is closer to the intended final hardware for this build.

## Toolchain

- **`.ioc`** — used to generate code via STM32CubeMX (opened through CubeIDE,
  the project itself is not built through CubeIDE).
- **`.ini`** (`platformio.ini`) — used with PlatformIO for building, flashing,
  and debugging. Since this setup runs on a Blue Pill clone together with a
  ST-Link V2 clone, the toolchain/OpenOCD versions and USB driver handling
  needed some non-default adjustments to work reliably — details on that
  (and the reasoning behind each config line) are documented separately in
  [blue-pill-check](https://github.com/Sterben624/blue-pill-check), a
  reference repo built specifically to work through these quirks before
  bringing the setup into this project.

## Note

This README was drafted with the help of Claude, after the PlatformIO /
toolchain / debugger setup itself was already sorted out and working.