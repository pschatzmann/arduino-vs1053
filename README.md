# VS1053 library

This is a Arduino library for breakout boards using the **[VS1053 and VS1003 Chip](https://www.vlsi.fi/en/products/vs1053.html)** by VLSI Solution:
A powerful Ogg Vorbis / MP3 / AAC / WMA / FLAC / MIDI Audio Codec chip.<br/>

![vs1053](https://pschatzmann.github.io/arduino-vs1053/doc/vs1053.jpg)


The library enables the possibility to play audio files. Eg. it may be a base to build your own webradio player or different audio device. I recommend to use this library together with my [arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools) with the related [examples](https://github.com/pschatzmann/arduino-audio-tools/tree/main/examples/examples-vs1053).

This is an interesting way to output audio information for all __microcontrollers that do not support I2S__ or do not have the power or resources to decode audio files.

Functional highlights:

- __Output__ of decoded audio
- Reading of pcm data from __microphone__ / aux input
- Methods to manage __volume, treble, bass, earphones__ 
- Support for __real time MIDI__
- Flexible __SPI__ Design
- Functioinality implemented in arduino_vs1053 __namespace__ 
- Supports __Arduino__
- Supports __PlatformIO__
- Supports __cmake__

## Logger

The application has a built in logger that uses Serial as output. The default log level is Warning. You can initialize the Logger with 
```
VS1053Logger.begin(Serial, VS1053Info); // use VS1053Debug, VS1053Info, VS1053Warning, VS1053Error
```

## Documentation

Here is the [relevant class documentation](https://pschatzmann.github.io/arduino-vs1053/doc/html/annotated.html).

You can find further information in the [Wiki](https://github.com/pschatzmann/arduino-vs1053/wiki) and in [my blogs](https://www.pschatzmann.ch/home/tag/vs1053/)...

## Pinout

The major disadvantage of the VS1053 based modules is the number of pins that need to be connected and the fact that they might be named differently. The module is using __SPI__ to communicate, therefore consult the SPI documentation of your microcontroller to find the correct SPI pins.


|  VS1053  |   Comment                                   |
|----------|---------------------------------------------|
| SCK      | SPI Serial Clock                            |
| MISO     | SPI Master Input ← Slave Output             |
| MOSI     | SPI Master Output → Slave Input             |
| (X)CS    | SPI Chip Select/ Configured in Sketch       |
| (X)DCS   | Data Chip Select: Configured in Sketch      |
| DREQ     | Data Request: Configured in Sketch          |
| XRST     | Hardware Reset Pin: low=reset               |
| 5V       | Power Supply                                |
| GND      | Ground                                      |

- The XRST pin can be configured in the sketch or you can connect it to RST of your microcontroller: XRST must be high for the module to work.
- Some boards provide a SD card and therefore have an aditional CS pin.

## Credits

Based on library/applications:
* [baldram/ESP_VS1053_Library](https://github.com/baldram/ESP_VS1053_Library) by [Marcin Szałomski](https://github.com/baldram)
* [maniacbug/VS1053](https://github.com/maniacbug/VS1053) by [J. Coliz](https://github.com/maniacbug)
* [Esp-radio](https://github.com/Edzelf/Esp-radio) by [Ed Smallenburg](https://github.com/Edzelf)
* [smart-pod](https://github.com/MagicCube/smart-pod) by [Henry Li](https://github.com/MagicCube)

## License

Copyright (C) 2017<br/>
Licensed under GNU GPLv3
