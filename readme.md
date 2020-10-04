= Custom Piano Tuner

== Environment

=== apt packages

- libsdl2-dev (for compilation only)
- libsdl2-2.0-0 - SDL support
- libasound2 - underlying ALSA support

Todo: move to headless mode and blow away a pile of unneeded packages.

=== /boot/config.txt

```
# See https://rpf.io/configtxt
dtparam=audio=on

# todo: turn off video
```

=== USB device details

`dmesg`:

```
usb 1-1.2: new full-speed USB device number 3 using xhci_hcd
usb 1-1.2: New USB device found, idVendor=0d8c, idProduct=013c, bcdDevice= 1.00
usb 1-1.2: New USB device strings: Mfr=1, Product=2, SerialNumber=0
usb 1-1.2: Product: USB PnP Sound Device
usb 1-1.2: Manufacturer: C-Media Electronics Inc.      
input: C-Media Electronics Inc.       USB PnP Sound Device as /devices/platform/scb/fd500000.pcie/pci0000:00/0000:00:00.0/0000:01:00.0/usb1/1-1/1-1.2/1-1.2:1.2/0003:0D8C:013C.0001/input/input0
hid-generic 0003:0D8C:013C.0001: input,hidraw0: USB HID v1.00 Device [C-Media Electronics Inc.       USB PnP Sound Device] on usb-0000:01:00.0-1.2/input2
```

`lsusb`:

```
Bus 001 Device 003: ID 0d8c:013c C-Media Electronics, Inc. CM108 Audio Controller
```

`arecord -l`:

```
**** List of CAPTURE Hardware Devices ****
card 2: Device [USB PnP Sound Device], device 0: USB Audio [USB Audio]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
```

`arecord -L`:

```
sysdefault:CARD=Device
    USB PnP Sound Device, USB Audio
    Default Audio Device
hw:CARD=Device,DEV=0
    USB PnP Sound Device, USB Audio
    Direct hardware device without any conversions
plughw:CARD=Device,DEV=0
    USB PnP Sound Device, USB Audio
    Hardware device with all software conversions
```

=== Input test

```
$ arecord -D hw:CARD=Device,DEV=0 -c 1 -f S16_LE -r 48000 -d 1 -v output-test
Recording WAVE 'output-test' : Signed 16 bit Little Endian, Rate 48000 Hz, Mono
Hardware PCM card 2 'USB PnP Sound Device' device 0 subdevice 0
Its setup is:
  stream       : CAPTURE
  access       : RW_INTERLEAVED
  format       : S16_LE
  subformat    : STD
  channels     : 1
  rate         : 48000
  exact rate   : 48000 (48000/1)
  msbits       : 16
  buffer_size  : 24000
  period_size  : 6000
  period_time  : 125000
  tstamp_mode  : NONE
  tstamp_type  : MONOTONIC
  period_step  : 1
  avail_min    : 6000
  period_event : 0
  start_threshold  : 1
  stop_threshold   : 24000
  silence_threshold: 0
  silence_size : 0
  boundary     : 1572864000
  appl_ptr     : 0
  hw_ptr       : 0
```





