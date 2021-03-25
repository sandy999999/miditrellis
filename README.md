*Work in progess*

# miditrellis
Firmware for midi controller with Adafruit Neotrellis and Teensy 4.0

2 x Neotrellis boards (X = 8, Y = 4)

3 x push-buttons with external pullup-resistors

1 x custom lego casing


Neotrellis tiling is upside down because of cables


Pressing the white button changes the Neotrellis view from Keyboard view to Sequencer view. 
The sequencer is a step-sequencer and inputs the last note pressed in Keyboard view at the chosen location in a 32 step sequence.
Step sequencer syncs to DAW and steps are synced to 1/8th notes.

/* Trellis buttons:
 *    0x30          0x2F
 * 27-26-25-24   31-30-29-28
 * 19-18-17-16   23-22-21-20
 * 11-10-09-08   15-14-13-12
 * 03-02-01-00   07-06-05-04
 * 
 * Translated into
 * 24-25-26-27-28-29-30-31
 * 16-17-18-19-20-21-22-23
 * 08-09-10-11-12-13-14-15
 * 00-01-02-03-04-05-06-07
 * 
 * 
 * Button setup
 * Green button - Play sequence
 * Red button - Stop playing
 * White button - Switch view (keyboard -> sequence)
 * 
 * Views:
 * - Keyboard (2 keyboard rows, western scale)
 * - Step sequencer
