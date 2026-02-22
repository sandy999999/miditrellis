/* Neotrellis + Teensy MIDI Controller

   Author: Andreas Sandersen
   2021 — Refactored 2026

   Hardware:
     2× Adafruit NeoTrellis (I2C: 0x30, 0x2F) — mounted upside-down
     Teensy 4.0 (USB MIDI device)
     3× push-buttons: Green (play), Red (stop), White (view toggle)

   Trellis physical → logical key mapping (boards mounted upside-down):
     Physical:                 Logical (after translateKey):
       0x30       0x2F           24-25-26-27-28-29-30-31  (top row)
     27-26-25-24  31-30-29-28   16-17-18-19-20-21-22-23
     19-18-17-16  23-22-21-20   08-09-10-11-12-13-14-15
     11-10-09-08  15-14-13-12   00-01-02-03-04-05-06-07  (bottom row)
     03-02-01-00  07-06-05-04

   Views (toggle with White button):
     Keyboard  — 2 rows of chromatic keyboard, octave shift on meta-keys
     Sequencer — 32-step gate+pitch sequencer, synced to MIDI clock (1/8-note steps)

   Sequencer usage:
     - In Keyboard view, press a note to set selectedNote
     - Switch to Sequencer view — press pads to activate steps
     - Each activated step stores the current selectedNote
     - Steps can hold different notes (select note in Keyboard view, switch, tap step)
     - Green button sends MIDI Start; Red button sends MIDI Stop + all-notes-off
     - Syncs to DAW via MIDI Clock, Start, Stop, Song Position Pointer
*/

#include "Adafruit_NeoTrellis.h"
#include <Bounce2.h>

// ── Constants ──────────────────────────────────────────────────────────────
#define MIDI_CHANNEL     0    // MIDI channel 1 (Teensy usbMIDI is 0-indexed)
#define INT_PIN         14    // NeoTrellis interrupt line
#define GREEN_PIN        1    // Play button
#define RED_PIN          2    // Stop button
#define WHITE_PIN        3    // View toggle button
#define Y_DIM            4    // Trellis rows
#define X_DIM            8    // Trellis columns
#define NUM_KEYS        (Y_DIM * X_DIM)   // 32
#define NUM_STEPS        32
// MIDI clock: 24 PPQN. 1/8-note = 12 clocks. Adjust here to change step resolution.
#define CLOCKS_PER_STEP 12

// ── Hardware ───────────────────────────────────────────────────────────────
Bounce2::Button greenButton  = Bounce2::Button();
Bounce2::Button redButton    = Bounce2::Button();
Bounce2::Button whiteButton  = Bounce2::Button();

Adafruit_NeoTrellis t_array[Y_DIM / 4][X_DIM / 4] = {
  { Adafruit_NeoTrellis(0x30), Adafruit_NeoTrellis(0x2F) }
};
Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM / 4, X_DIM / 4);

// ── Global state ───────────────────────────────────────────────────────────
bool seqView     = false;
bool playing     = false;

// Keyboard
// semitones[i]: interval from octave root. Negative = meta-key (octave shift / unused slot).
int semitones[16] = {
    0,  2,  4,  5,  7,  9, 11, -3,   // bottom row (keys 0–7): C D E F G A B [oct-]
   -1,  1,  3, -1,  6,  8, 10, -2,   // top row    (keys 8–15): [x] C# D# [x] F# G# A# [oct+]
};
byte octaveBaseNotes[] = { 24, 36, 48, 60, 72, 84, 96, 108 };
int topOctave    = 3;   // row keys 16–31
int bottomOctave = 1;   // row keys 0–15
int selectedNote = 60;  // last note pressed in Keyboard view (default: middle C)

// Sequencer
int  clockCount     = 0;
int  sequenceCount  = -1;   // will increment to 0 on the first MIDI clock after Start
int  lastStep       = -1;   // which step LED to restore when cursor advances
int  sequence[NUM_STEPS];        // 1 = active, 0 = inactive
int  sequenceNotes[NUM_STEPS];   // MIDI note stored per step

// ── Forward declarations ───────────────────────────────────────────────────
void updateKeys();
void allNotesOff();
void setStepPixel(int logicalKey, bool isCursor);
uint32_t Wheel(byte pos);
uint32_t noteColor(int midiNote);
void onClock();
void onStart();
void onStop();
void onSongPosition(uint16_t semiQ);

// ── Key translation ────────────────────────────────────────────────────────
// Boards are mounted upside-down; each group of 4 keys per row is reversed.
// key ^ 3 flips the two lowest bits, which mirrors groups of 4 — identical
// to the original 32-case switch but without the dead-code risk.
int translateKey(int key) {
  return key ^ 3;
}

// ── Color helpers ──────────────────────────────────────────────────────────
uint32_t Wheel(byte pos) {
  pos = 255 - pos;
  if (pos < 85)  return seesaw_NeoPixel::Color(255 - pos * 3, 0, pos * 3);
  if (pos < 170) { pos -= 85; return seesaw_NeoPixel::Color(0, pos * 3, 255 - pos * 3); }
  pos -= 170;    return seesaw_NeoPixel::Color(pos * 3, 255 - pos * 3, 0);
}

// Map a MIDI note (0–127) to a unique hue so each note has a consistent colour.
uint32_t noteColor(int midiNote) {
  return Wheel((byte)map(midiNote, 0, 127, 0, 255));
}

// Set a keyboard key to its resting colour (octave-tinted or off for meta-keys).
void offColor(int logicalKey) {
  int physKey = translateKey(logicalKey);
  int octave  = (logicalKey > 15) ? topOctave : bottomOctave;
  if (semitones[logicalKey % 16] < 0) {
    trellis.setPixelColor(physKey, 0);
  } else {
    trellis.setPixelColor(physKey, Wheel(octaveBaseNotes[octave]));
  }
}

// Draw one sequencer step: red cursor if isCursor, note colour if active, off if inactive.
void setStepPixel(int logicalKey, bool isCursor) {
  int physKey = translateKey(logicalKey);
  if (isCursor) {
    trellis.setPixelColor(physKey, 0xFF0000);
  } else if (sequence[logicalKey]) {
    trellis.setPixelColor(physKey, noteColor(sequenceNotes[logicalKey]));
  } else {
    trellis.setPixelColor(physKey, 0);
  }
}

// ── Trellis callback ───────────────────────────────────────────────────────
TrellisCallback blink(keyEvent evt) {
  int physKey    = evt.bit.NUM;
  int logicalKey = translateKey(physKey);
  int octave     = (logicalKey > 15) ? topOctave : bottomOctave;

  if (!seqView) {
    // ── Keyboard view ──────────────────────────────────────────────────────
    if (semitones[logicalKey % 16] >= 0) {
      int note = semitones[logicalKey % 16] + octaveBaseNotes[octave];
      if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
        trellis.setPixelColor(physKey, Wheel(random(256)));
        selectedNote = note;
        usbMIDI.sendNoteOn(note, 100, MIDI_CHANNEL);
      } else {
        offColor(logicalKey);
        usbMIDI.sendNoteOff(note, 0, MIDI_CHANNEL);
      }
    } else {
      // Meta-keys: octave shift
      if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
        if (logicalKey == 31) topOctave    = constrain(topOctave    + 1, 0, 7);
        if (logicalKey == 23) topOctave    = constrain(topOctave    - 1, 0, 7);
        if (logicalKey == 15) bottomOctave = constrain(bottomOctave + 1, 0, 7);
        if (logicalKey ==  7) bottomOctave = constrain(bottomOctave - 1, 0, 7);
        updateKeys();
        return 0;  // updateKeys() already called trellis.show()
      }
    }

  } else {
    // ── Sequencer view ─────────────────────────────────────────────────────
    if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
      if (sequence[logicalKey]) {
        // Deactivate step
        sequence[logicalKey] = 0;
      } else {
        // Activate step — store current selectedNote
        sequence[logicalKey]      = 1;
        sequenceNotes[logicalKey] = selectedNote;
      }
      // Don't overwrite the red playhead cursor
      if (!(playing && logicalKey == sequenceCount)) {
        setStepPixel(logicalKey, false);
      }
    }
  }

  trellis.show();
  return 0;
}

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  pinMode(INT_PIN, INPUT_PULLUP);

  greenButton.attach(GREEN_PIN,  INPUT); greenButton.interval(5);
  redButton.attach(RED_PIN,      INPUT); redButton.interval(5);
  whiteButton.attach(WHITE_PIN,  INPUT); whiteButton.interval(5);

  if (!trellis.begin()) {
    Serial.println("NeoTrellis init failed — check wiring and I2C addresses");
    while (1) delay(10);
  }
  Serial.println("NeoTrellis ready");

  // Startup animation
  for (uint16_t i = 0; i < NUM_KEYS; i++) {
    trellis.setPixelColor(translateKey(i), Wheel(map(i, 0, NUM_KEYS - 1, 0, 255)));
    trellis.show();
    delay(40);
  }

  // Initialise sequence arrays
  for (int i = 0; i < NUM_STEPS; i++) {
    sequence[i]      = 0;
    sequenceNotes[i] = 60;  // default: middle C
  }

  // Activate all keys and register callback (once, after loop)
  for (int i = 0; i < NUM_KEYS; i++) {
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING,  true);
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_FALLING, true);
    trellis.registerCallback(i, blink);
  }

  usbMIDI.setHandleSongPosition(onSongPosition);
  usbMIDI.setHandleClock(onClock);
  usbMIDI.setHandleStart(onStart);
  usbMIDI.setHandleStop(onStop);

  updateKeys();
}

// ── Main loop ──────────────────────────────────────────────────────────────
void loop() {
  if (whiteButton.pressed()) {
    seqView = !seqView;
    updateKeys();
  }
  if (greenButton.pressed()) {
    usbMIDI.sendRealTime(usbMIDI.Start);
  }
  if (redButton.pressed()) {
    usbMIDI.sendRealTime(usbMIDI.Stop);
    allNotesOff();
  }

  greenButton.update();
  redButton.update();
  whiteButton.update();

  if (!digitalRead(INT_PIN)) trellis.read();

  while (usbMIDI.read()) {}
}

// ── Display refresh ────────────────────────────────────────────────────────
void updateKeys() {
  if (seqView) {
    for (int i = 0; i < NUM_STEPS; i++) {
      setStepPixel(i, playing && (i == sequenceCount));
    }
  } else {
    for (int i = 0; i < NUM_KEYS; i++) {
      offColor(i);
    }
  }
  trellis.show();
}

// ── MIDI utility ───────────────────────────────────────────────────────────
void allNotesOff() {
  for (int i = 0; i < 128; i++) {
    usbMIDI.sendNoteOff(i, 0, MIDI_CHANNEL);
  }
}

// ── MIDI clock handlers ────────────────────────────────────────────────────

// Called 24× per quarter note by the DAW.
// Steps fire every CLOCKS_PER_STEP clocks (default 12 = 1/8-note resolution).
void onClock() {
  if (clockCount == 0) {
    // Restore the LED for the step that just finished
    if (lastStep >= 0 && seqView) {
      setStepPixel(lastStep, false);
    }

    // Send note-off for the previous step's note
    if (lastStep >= 0 && sequence[lastStep]) {
      usbMIDI.sendNoteOff(sequenceNotes[lastStep], 0, MIDI_CHANNEL);
    }

    // Advance to next step (wraps at NUM_STEPS)
    sequenceCount = (sequenceCount + 1) % NUM_STEPS;

    // Send note-on if this step is active
    if (sequence[sequenceCount]) {
      usbMIDI.sendNoteOn(sequenceNotes[sequenceCount], 100, MIDI_CHANNEL);
    }

    // Update playhead LED
    if (seqView) {
      setStepPixel(sequenceCount, true);  // red cursor
      trellis.show();
    }

    lastStep = sequenceCount;
  }

  clockCount = (clockCount + 1) % CLOCKS_PER_STEP;
}

void onStart() {
  clockCount    = 0;
  sequenceCount = -1;   // onClock will increment to 0 on the first tick
  lastStep      = -1;
  playing       = true;
  if (seqView) updateKeys();
}

void onStop() {
  playing = false;
  allNotesOff();
  if (seqView) updateKeys();  // clear the red cursor
}

// Reposition the sequencer to match DAW playback position.
// semiQ is in MIDI 1/16-note beats (6 MIDI clocks each).
void onSongPosition(uint16_t semiQ) {
  long totalClocks  = (long)semiQ * 6;
  clockCount        = (int)(totalClocks % CLOCKS_PER_STEP);
  sequenceCount     = (int)((totalClocks / CLOCKS_PER_STEP) % NUM_STEPS);
  lastStep          = sequenceCount;
  if (seqView) updateKeys();
}
