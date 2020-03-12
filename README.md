# Aquarium Sensor Monitor

I have had aquariums for a while, so I decided one day to add some specific sensors to help me keep track of the
health of the aquarium over time. Atlas makes some really nice, if not expensive, water sensors than can be used in both
fresh and saltwater tanks. For my freshwater tank, I plan to add dissolved oxygen and a pH sensor to help me keep track
of what's going on during the day. I also plan to add a temperature monitor and have added support for keeping track 
of the flow rate for the pump and a water level sensor to make sure I have water in there. The goal is to give me 
some insight into tank health as time goes by, and provide me with rapid warning if something critically bad happens.

To do this, I've created a board to hold a Rasperry PI Zero W, along with space to add the Atlas sensors and a ADC to 
help me keep track of water level. It is a somewhat complicated 4 layer PCB that includes the PI, three Atlas sensor installations, two temp probes, a couple of random GPIO (to control whatever), and an 8 channel ADC, though currently 
I only use 1. They don't make a through hole 1 channel ADC that I can find.

This is also designed to work with my Android Things Aquarium display, which will give me some visual feedback when 
wanted about the state of the tank. It will also give me the ability to track cleanings, filter changes, and a list 
of other things that should be tracked over time to avoid letting them go too long.

## Installation

You need a PI zero, with build-essentials and cmake. You compile it doing the following

```
mkdir build
cd build
cmake ..
make
cp app/aquarium /usr/bin
```

You must copy the aquarium.config file to /root/.config, as this app MUST run as root. It's easiest if you start 
it from rc.local as the last item in the list. It has a function to wait on the network, so that shouldn't be too 
big a problem. If the MQTT server doesn't come up in 5 minutes, it will fail. The whole point of this is MQTT 
access to data. It also can talk to AdafruitIO, but doesn't block waiting for that. If that doesn't connect, then 
you can't talk to AdafruitIO.

I won't got into detail about how to build out the hardware right now.

## Configuration

I will describe how to populate the config file here soon.
