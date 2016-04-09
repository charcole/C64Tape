# C64Tape

Homebrew device for loading .tap files from an SD card on a real Commodore 64. It connects to the C64's cassette port and acts like a Commodore Datasette (1530). Instead of real tapes though you can load compressed tap files (tape images used by emulators) off the SD card.

Video of it in action is here:
https://www.youtube.com/watch?v=QHEHTlryBxw

Components
- Teensy 3.2 (Massively overkill, any reasonably quick 3.3V Arduino compatible would do)
- Microchip 23LC1024 128Kb Serial SRAM
- SD card module from eBay (Only used as it was easy to solder)
- Couple 10KOhm resistors
- Push Button

Schematic in ASCII art at top of Arduino source.

Tap files are approximately 8 times larger than the data they represent so tend to be pretty large. Using a simple LZ77 like compressor I got approximately a 10 to 1 reduction in file size. This combined with reloading new data whenever there's a long pause (>1s) means that we can get away with a relatively small amount of SRAM (128Kb) even for tap files several megabytes long. c64compress.c needs to be used to compress the files before you put them on the SD card to allow C64Tape to read them.

User interface: Short button presses switch between different tap files on the SD card. A long press plays the tape.
