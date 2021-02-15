/* Neotrellis + Teensy Midi controller 

  Author: Andreas Sandersen
  2021

*/

#include "Adafruit_NeoTrellis.h"
#include <Bounce2.h>


#define MIDI_CHANNEL 0

#define INT_PIN 14 // interrupt signal from neotrellis
#define GREEN_PIN 1
#define RED_PIN 2
#define WHITE_PIN 3

Bounce2::Button greenButton = Bounce2::Button();
Bounce2::Button redButton = Bounce2::Button();
Bounce2::Button whiteButton = Bounce2::Button();

#define Y_DIM 4 //number of rows of trellis-keys
#define X_DIM 8 //number of columns of trellis-keys

//create a matrix of trellis panels
Adafruit_NeoTrellis t_array[Y_DIM/4][X_DIM/4] = {
  
  { Adafruit_NeoTrellis(0x2F), Adafruit_NeoTrellis(0x30) }
  
};

//pass this matrix to the multitrellis object
Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM/4, X_DIM/4);

/* // Grid setup:
 *    0x30          0x2F
 * 16-15-14-13   16-15-14-13
 * 12-11-10-09   12-11-10-09
 * 08-07-06-05   08-07-06-05
 * 04-03-02-01   04-03-02-01
 * 
 * 
 * Setup
 * State 0 - Keyboard, 2 keyboard rows
 * State 1 - Select note -> press white button to show 32 step Sequencer -> input note at steps -> press white button again to exit
 */

int state = 0;

int semitones[16] = {
    0,  2, 4,  5, 7, 9, 11, -3,
    -1, 1, 3, -1, 6, 8, 10, -2,
};


byte octaveBaseNotes[] = {
    24, 36, 48, 60, 72, 84, 96, 108,
};

int topOctave = 3, bottomOctave = 1; // Just pleasant defaults

#define RED_DIM 0x190000

void offColor(int key, int octave) {
  // Very dirty switch case hacks as my Neotrellis boards are upside down because of cables
  int trellisKey = 0;
  switch (key) {
      case 0:
        trellisKey = 7;
        break;
      case 1:
        trellisKey = 6;
        break;
      case 2:
        trellisKey = 5;
        break;
      case 3:
        trellisKey = 4;
        break;
      case 4:
        trellisKey = 3;
        break;
      case 5:
        trellisKey = 2;
        break;
      case 6:
        trellisKey = 1;
        break;
      case 7:
        trellisKey = 0;
        break;
      case 8:
        trellisKey = 15;
        break;
      case 9:
        trellisKey = 14;
        break;
      case 10:
        trellisKey = 13;
        break;
      case 11:
        trellisKey = 12;
        break;
      case 12:
        trellisKey = 11;
        break;
      case 13:
        trellisKey = 10;
        break;
      case 14:
        trellisKey = 9;
        break;
      case 15:
        trellisKey = 8;
        break;
      case 16:
        trellisKey = 23;
        break;
      case 17:
        trellisKey = 22;
        break;
      case 18:
        trellisKey = 21;
        break;
      case 19:
        trellisKey = 20;
        break;
      case 20:
        trellisKey = 19;
        break;
      case 21:
        trellisKey = 18;
        break;
      case 22:
        trellisKey = 17;
        break;
      case 23:
        trellisKey = 16;
        break;
      case 24:
        trellisKey = 31;
        break;
      case 25:
        trellisKey = 30;
        break;
      case 26:
        trellisKey = 29;
        break;
      case 27:
        trellisKey = 28;
        break;
      case 28:
        trellisKey = 27;
        break;
      case 29:
        trellisKey = 26;
        break;
      case 30:
        trellisKey = 25;
        break;
      case 31:
        trellisKey = 24;
        break;
  }
  if (semitones[key % 16] < 0) {
    trellis.setPixelColor(trellisKey, 0);
  } else {
    trellis.setPixelColor(trellisKey, Wheel(octaveBaseNotes[octave]));
  }
}

TrellisCallback blink(keyEvent evt){
    int trellisKey = evt.bit.NUM;
    int midiKey = 0;
    switch (trellisKey) {
      case 0:
        midiKey = 7;
        break;
      case 1:
        midiKey = 6;
        break;
      case 2:
        midiKey = 5;
        break;
      case 3:
        midiKey = 4;
        break;
      case 4:
        midiKey = 3;
        break;
      case 5:
        midiKey = 2;
        break;
      case 6:
        midiKey = 1;
        break;
      case 7:
        midiKey = 0;
        break;
      case 8:
        midiKey = 15;
        break;
      case 9:
        midiKey = 14;
        break;
      case 10:
        midiKey = 13;
        break;
      case 11:
        midiKey = 12;
        break;
      case 12:
        midiKey = 11;
        break;
      case 13:
        midiKey = 10;
        break;
      case 14:
        midiKey = 9;
        break;
      case 15:
        midiKey = 8;
        break;
      case 16:
        midiKey = 23;
        break;
      case 17:
        midiKey = 22;
        break;
      case 18:
        midiKey = 21;
        break;
      case 19:
        midiKey = 20;
        break;
      case 20:
        midiKey = 19;
        break;
      case 21:
        midiKey = 18;
        break;
      case 22:
        midiKey = 17;
        break;
      case 23:
        midiKey = 16;
        break;
      case 24:
        midiKey = 31;
        break;
      case 25:
        midiKey = 30;
        break;
      case 26:
        midiKey = 29;
        break;
      case 27:
        midiKey = 28;
        break;
      case 28:
        midiKey = 27;
        break;
      case 29:
        midiKey = 26;
        break;
      case 30:
        midiKey = 25;
        break;
      case 31:
        midiKey = 24;
        break;
    }

    int octave;

    if (midiKey > 15) {
      octave = topOctave;
    } else {
      octave = bottomOctave;
    }
    
    if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
      trellis.setPixelColor(trellisKey, Wheel(random(255)));
    } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
      offColor(midiKey, octave);
    }

    if (semitones[midiKey % 16] >= 0) {
      if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
        usbMIDI.sendNoteOn(semitones[midiKey % 16] + octaveBaseNotes[octave], 64, MIDI_CHANNEL);
        Serial.printf("Note On %i\n", semitones[midiKey % 16] + octaveBaseNotes[octave]);
      } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
        usbMIDI.sendNoteOff(semitones[midiKey % 16] + octaveBaseNotes[octave], 64, MIDI_CHANNEL);
        Serial.printf("Note Off %i\n", semitones[midiKey % 16] + octaveBaseNotes[octave]);
      }
    } else {
      // metakeys!
      if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
        if (midiKey == 31)
          topOctave++;
        if (midiKey == 23)
          topOctave--;
        if (midiKey == 15)
          bottomOctave++;
        if (midiKey == 7)
          bottomOctave--;
          
        if (topOctave > 7)
          topOctave = 7;
        if (bottomOctave > 7)
          bottomOctave = 7;
        if (topOctave < 0)
          topOctave = 0;
        if (bottomOctave < 0)
          bottomOctave = 0;

        for (int i = 0; i < 32; i++)
          offColor(i, i > 15 ? topOctave : bottomOctave);
      }
    }
  
  trellis.show();
  return 0;
}


void setup() {
  Serial.begin(115200);
  //while (!Serial);
  pinMode(INT_PIN, INPUT_PULLUP);
  
  //Button setup
  greenButton.attach( GREEN_PIN, INPUT ); 
  greenButton.interval(5);
  redButton.attach( RED_PIN, INPUT ); 
  redButton.interval(5);
  whiteButton.attach( WHITE_PIN, INPUT );
  whiteButton.interval(5);

  //Trellis setup
  if (!trellis.begin())
  {
    Serial.println("failed to begin trellis");
    while (1);
  }
  else{
    Serial.println("trellis started");
  }

  //do a little animation to show we're on
  for(uint16_t i=0; i<Y_DIM*X_DIM; i++) {
    trellis.setPixelColor(i, Wheel(map(i, 0, Y_DIM*X_DIM, 0, 255)));
    trellis.show();
    delay(50);
  }
  //activate all keys and set callbacks
  for(int i=0; i<Y_DIM*X_DIM; i++){
    offColor(i, i < 16 ? topOctave : bottomOctave);
    trellis.show();
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING, true);
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_FALLING, true);
    trellis.registerCallback(i, blink);
  }
}


void loop() {
  if ( whiteButton.pressed() ) {
    Serial.printf("white button pressed \n");
  }

  greenButton.update();
  redButton.update();
  whiteButton.update();

  if ( greenButton.pressed() ) {  

  }
  if ( redButton.pressed() ) {
    
  }

  if(!digitalRead(INT_PIN)){
    trellis.read();
  }
  
  while (usbMIDI.read()) {
    // ignore incoming messages
  }

}


uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return seesaw_NeoPixel::Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return seesaw_NeoPixel::Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return seesaw_NeoPixel::Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
