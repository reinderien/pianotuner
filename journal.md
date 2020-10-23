### Dec 20, 2015

I started a rudimentary, software-only piano tuner experiment.

### Dec 21, 2015

Abandoned the software experiment.

### July 4, 2019

I hack together some notes for a possible hardware implementation of a piano
tuner, thinking about:

- Needles for octave, note in octave, tuning in note, volume
- Lamp for power
- Lamps behind needle dials
- Capacitive fade-in for lamps
- Glass or plastic bead in front of power lamp
- Phonograph-style mic pickup
- Clock hands for needles

Initially I assume it'll run a Fast Fourier Transform, and toy with the idea
of using a 
[Texas Instruments LEA](https://www.ti.com/lit/an/slaa720/slaa720.pdf)
(Low-Energy Accelerator). That would be good if we're making 10,000 of these
things, but it would be a steep learning curve.

All of this goes into a Google Doc and then somewhat forgotten about.

### Sept 26, 2020

I found an antique-style wooden box on Facebook Marketplace. It's pretty
clearly one-of-a-kind, and was hand-made in an Egyptian workshop by a late 
relative of the seller. It's perfect.

In around now, plotting starts to solidify for a hardware tuner.

### Sept 28, 2020

Wife orders 
[copper foil](https://www.amazon.ca/gp/product/B0042SWYUA) 
for the horn. I order the 
[USB microphone](https://www.amazon.ca/gp/product/B07VWJ1XB2) 
that will be connected to the Raspberry Pi 4 I have on hand.

### Sept 29, 2020

Orders for more power accessories:
[5V 25W power supply](https://www.amazon.ca/gp/product/B00DECXUD0), 
and a nice little 
[USB-C-to-terminal-block power adapter](https://www.amazon.ca/gp/product/B07R8YPFD7)
to power the Pi without having to jankily connect a cell phone charger on the
inside of the enclosure.

Ordered a handful of gauges, 5Vdc analogue, of various sizes:

[45mm](https://www.ebay.ca/itm/181981812727),
[66mm](https://www.ebay.ca/itm/183445575857)
and
[90mm](https://www.ebay.ca/itm/172695600128).

These are coming from China so I'm expecting it to take forever for them to 
get here, but anything in North America is exhorbitant. The intent is to 
replace the faceplate with a custom one and backlight it if possible.

Order some vintage-style electrical accessories from 
[Vintage Wire & Supply](https://vintagewireandsupply.com). They're relatively
expensive but look incredible.

### Oct 2, 2020

Going on vacation. The wife has let me bring a pile of tech goodies to work on 
this project.

### Oct 3, 2020

Using the same GitHub web page, resuscitate the tuner project, but with 
hardware in mind. The Raspberry Pi, keyboard, mouse, microphone and cell phone
power supply are set up on the coffee table of the cottage, and I have a great
time getting going on development.

### Oct 4, 2020

Start using the Simple Direct Layer for microphone capture on the Raspberry Pi.
Pretty much immediately throw that out the window in favour of the underlying
Advanced Linux Sound Architecture.

### Oct 7, 2020

Research into the best way to detect pitch. Intend on running autocorrelation 
for note detection, in line with the
[findings of Coert Vonk](https://coertvonk.com/sw/embedded/arduino-pitch-detector-13252),
and associated research by
[Brown](http://academics.wellesley.edu/Physics/brown/pubs/acptrv89P2346-P2354.pdf)
and
[Monti](http://193.166.3.2/pub/sci/audio/dafx/2000/profs.sci.univr.it/%257Edafx/Final-Papers/pdf/Monti_DAFX00poster.pdf).

Some of it makes no sense to me so I post on a 
[Digital Signal Processing](https://dsp.stackexchange.com/questions/70736)
forum.

Ordered some Microchip PIC16LF1773 (programmable interrupt controller) from 
Mouser, since Microchip was nearly out of stock. These devices have a mix of 5- 
and 10-bit unbuffered DACs that can be internally buffered with three op-amps; 
they also support UART, I2C and SPI - so one way or the other they'll 
definitely be able to talk with the Pi.

### Oct 10, 2020

Start using the Gnu Scientific Library with the intent of applying it to the 
autocorrelation work.

### Oct 11, 2020

Throw GSL out the window in favour of ATLAS (Automatically Tuned Linear Algebra 
Software), an implementation of BLAS (Basic Linear Algebra Subprograms).

### Oct 16, 2020

The gauges arrived from China, quicker than I expected! They're wonderful
little devices, fully disassemble-able with two screws on the side, and come
pre-packaged with hardware for the mounting bolts. There's a high-accuracy
resistor in each, and the input impedance is a little over 5k, so the PIC can
definitely handle it.

### Oct 19, 2020

Start a PIC firmware project to act as a DAC (digital-to-analogue converter) 
for the Raspberry Pi.

### Oct 21, 2020

Start an Eagle schematic to plan out how the DAC will be connected.

### Oct 22, 2020

Parts I still need to dig up or buy:

- Amber LEDs for the gauge backlights
- Perfboard
- Rectangular cable to run from the Pi to header on the perfboard
- A rail-to-rail 0-5V buffer op-amp for the one remaining unbuffered DAC output

I also need to upgrade the Pi to a 64-bit operating system, since I didn't 
realize I had been using a deprecated 32-bit Raspbian.

Also need to write a calibration routine that gives me interquartile range for
the harmonics at various notes.

### Oct 23, 2020

Consider using a nice amber LED for backlights; such as the Lite-On LTL-1CHA with
- 3mm diffusion lens
- 20mA max, 2.1 volts forward
- 60° half-power angle
- Wavelengths: 602 nanometres dominant, 610 nanometres peak

If we use 10mA, we need 270R limiting each LED. For fade-in with a capacitor parallel to the LED, and a time constant over 2 seconds, we need a large capacitor of ~10mF.
