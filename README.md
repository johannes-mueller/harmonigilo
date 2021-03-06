# Harmonigilo
Work in progress / don't use yet, just look and test


## What it is
(or what it is supposed to be)

*Harmonigilo* is a LV2 plugin designed to enhance solo vocal audio tracks by
making them sound more voluminous. This is achieved by the following measures:

* The signal is split up into six parallel signals called voices

* These voices are slightly (a couple of cents) pitch shifted up
  and/or down

* Then the voices are slightly (some 15 milliseconds) delayed

* Finally the voices are panned two the stereo panorama

* The final stereo signal is blended with the input signal

* The GUI uses RobTk by Robin Gareus aka x42


## Usage
There are the following controls

* Enabled 1-6 (enables/disables the voices)

* Pitch 1-6 (the pitch shifts)

* Delay 1-6 (the delays)

* Pan 1-6 (the panning of the voices)

* Gain 1-6 (the levels of the voices)

* Dry Pan (the panning of the dry signal)

* Dry Gain (the gain of the dry signal)

Moreover each voice as well as the dry signal has a mute and solo button. The
difference between muting and disabling a voice is, that muting just mutes the
voice but the voice remains processed. Whereas disabling a voice means, that
the voice is no longer processed. This difference is important because
processing has an influence on the latency of the plugin. So disable a voice,
if you don't need it at all. Mute it, if you just want to check what it sounds
like without that specific voice.

## Todo

* Test'n'Debug

* Make the GUI look nicer

* Make it run on Windows and Mac (I probably won't do it, so PRs welcome)

* Switch to *wafscript*. Or stay with *make*? Don't know.

* Write more docs


## What does the name mean?

It's Esperanto. You can probably guess what it means. Maybe it should be named "korusigilo".
