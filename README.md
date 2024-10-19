# OMOTHA9 DCC Sound Decoder

<H2>Introduction</H2>
I am a beginner who has been doing DCC model railroading for 4 years. This project initially started with ESP32. It was 4 years ago. Around that time, fujigaya released a sound library for RP2040. I used it to install a DCC decoder on an N gauge. It was a fun experience.
<BR><BR>
In Japan, DCC is not common, and analog 12V DC is common. I thought that the reason DCC is not common is that the barrier to entry is high. In other words, the command station and decoder are expensive.
<BR><BR>
My hope is that MTC21 and NEXT18 connectors will be available for model railroads sold in Japan. When that is achieved, I expect this hobby to spread greatly.
<BR><BR>
For that Japan needs DCC user more and more.
<BR><BR>
When I thought about what I could do, I came up with the idea of ​​developing an inexpensive DCC vehicle with sound. This is the first one, an introductory version to let you know what DCC is. Therefore, it is a minimum necessary hardware configuration and easy-to-understand software.
<BR><BR>
I am thinking of having DCC experts assemble this and provide it to beginners at a reasonable cost.
<BR>
<H2>Software</H2>
<BR>Copyright(C)'2022 - 2024 HMX
<BR>This software is released under the MIT License, see LICENSE.txt
<BR>
<BR>This software uses, diverts or references the following software.
<BR>Please confirm the original text for each license.
<BR><BR>
I am grateful for the development of these libraries and for allowing their use.
<H3>Dcc Decoder</H3>
In addition to the NMRA library, we also use the DesktopStation RP2040 library.
<BR>file:     NmraDcc.cpp
<BR>file:     NmraDcc.h
<BR>file:     NmraDccMultiFunctionMotorDecoder.ino
<BR>author:   Alex Shepherd
<BR>license:  LGPL-2.1 license
<BR>
<BR>file:     NmraDcc_pio.cpp
<BR>file:     NmraDcc_pio.h
<BR>file:     dcc_pulse_dec.h
<BR>author:   Desktop Station
<BR>license:  Open source(Not specified)
<BR>
<BR>file:     DccCV.h
<BR>author:   Ayanosuke(Maison de DCC) / Desktop Station
<BR>license:  MIT License
<BR>
This uses the library for RP2040 created by fujigaya san.
<H3>RP2040 Sound Generator</H3>
<BR>file:     Timer_test2_752_31.ino
<BR>author:   fujigaya
<BR>license:  Open source(Not specified)

<H2>Board Design</H2>
The MPU uses the PR2040-ZERO (compatible). It is a development board equipped with the PR2040. The sound amplifier is the PAM8302A, and the motor driver is the BDR6133. It can support up to 13 LEDs in total, with 6 on the front and back, and 1 on the PR2040-ZERO.

