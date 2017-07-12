# Harmonigilo
Work in progress / don't use yet, just look and test


## What it is
(or what it is supposed to be)

*Harmonigilo* is a LV2 plugin designed to enhance solo vocal audio tracks by
making them sound more voluminous. This is achieved by the following measures:

* The signal is split up into two parallel signals

* These parallel signals are slightly (a couple of cents) pitch shifted up
  and/or down

* Then the parallel signals are slightly (some 15 milliseconds) delayed

* Finally the parallel signals are panned two the stereo panorama

* The final stereo signal is blended with the input signal


## Usage
So far there's only the generic UI. There are the following controls

* Pitch L / Pitch R (the two pitch shifts)

* Delay L / Delay R (the two delays)

* Panner width (the stereo width 0 = no stereo; 1 = left/right strictly
  seperated)

* Dry/Wet (the blend with the original signal 0 = totally dry, 1 = only wet)


## Todo

* Test'n'Debug

* Write a GUI

* Make it run on Windows and Mac (I probably won't do it, so PRs welcome)

* Switch to *wafscript*. Or stay with *make*? Don't know.


## What does the name mean?

It's Esperanto. You can probably guess what it means.
