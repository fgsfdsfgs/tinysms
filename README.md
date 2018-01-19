# TinySMS
![Screenshot](/screenshot.png?raw=true)

A tiny Win32 demo that recreates the JPN Sega Master System BIOS' attract mode.

Fits into 32k when UPX-compressed and into less than 256k when uncompressed. The VGM song embedded in the executable takes up probably 80% of that space.

### Requirements
* Visual C++ 2012 or newer (cl.exe and link.exe);
* UPX.

### Compiling
1. Ensure cl.exe, link.exe and upx.exe are in PATH.
2. Run build.bat.

### Credits
* Maxim (?) for the PSG emulator;
* Mitsutaka Okazaki for the YM2413 emulator.

The licensing status of these two is unclear.
