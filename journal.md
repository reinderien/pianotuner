### Dec 20, 2015

I started a rudimentary, software-only piano tuner experiment.

### Dec 21, 2015

Abandoned the software experiment.

### Sept 28, 2020

In around now, plotting starts to do a hardware tuner.

Wife orders 
[copper foil](https://www.amazon.ca/gp/product/B0042SWYUA) 
for the horn. I order the 
[USB microphone](https://www.amazon.ca/gp/product/B07VWJ1XB2/ref=ppx_yo_dt_b_asin_title_o04_s00?ie=UTF8&psc=1) 
that will be connected to the Raspberry Pi.

### Sept 29, 2020

Orders for more power accessories:
[5V 25W power supply](https://www.amazon.ca/gp/product/B00DECXUD0), 
and a nice little 
[USB-C-to-terminal-block power adapter](https://www.amazon.ca/gp/product/B07R8YPFD7)
to power the pi without having to jankily connect a cell phone charger on the
inside of the enclosure.

Ordered a handful of gauges, 5Vdc analogue, of various sizes:

todo.

### Oct 2, 2020

Going on vacation. The wife has let me bring a pile of tech goodies to work on 
this project.

### Oct 3, 2020

Using the same website, resuscitate the tuner project, but with hardware in
mind.

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

Some of it makes no sense to me so I posted on a 
[Digital Signal Processing](https://dsp.stackexchange.com/questions/70736)
forum.

### Oct 10, 2020

Start using the Gnu Scientific Library with the intent of applying it to the 
autocorrelation work.

### Oct 11, 2020

Throw GSL out the window in favour of ATLAS (Automatically Tuned Linear Algebra 
Software), an implementation of BLAS (Basic Linear Algebra Subprograms).

### Oct 19, 2020

Start a PIC (programmable interrupt controller) firmware project to act as a
DAC (digital-to-analogue converter) for the Raspberry Pi.

### Oct 22, 2020

Start an Eagle schematic to plan out how the DAC will be connected.
