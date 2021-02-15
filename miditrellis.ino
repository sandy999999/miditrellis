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
    -1, 1, 3, -1, 6, 8, 10, -2,
    0,  2, 4,  5, 7, 9, 11, -3,
};

byte octaveBaseNotes[] = {
    24, 36, 48, 60, 72, 84, 96, 108,
};

int topOctave = 1, bottomOctave = 3; // Just pleasant defaults

void offColor(int key, int octave) {
  if (semitones[key % 16] < 0) {
    trellis.setPixelColor(key, 0);
  } else {
    trellis.setPixelColor(key, Wheel(octaveBaseNotes[octave]));
  }
}


TrellisCallback blink(keyEvent evt){

    int key = evt.bit.NUM;
    int octave;

    if (key < 15) {
      octave = bottomOctave;
    } else {
      octave = topOctave;
    }
    if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
      trellis.setPixelColor(key, Wheel(random(255)));
    } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
      offColor(key, octave);
    }

    if (semitones[key % 16] >= 0) {
      if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
        usbMIDI.sendNoteOn(semitones[key % 16] + octaveBaseNotes[octave], 64, MIDI_CHANNEL);
      } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
        usbMIDI.sendNoteOff(semitones[key % 16] + octaveBaseNotes[octave], 64, MIDI_CHANNEL);
      }
    } else {
      // metakeys!
      if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
        if (key == 31)
          topOctave++;
        if (key == 23)
          topOctave--;
        if (key == 15)
          bottomOctave++;
        if (key == 7)
          bottomOctave--;
        // Limiting. I'm sure I could be more clever than this ...
        if (topOctave > 7)
          topOctave = 7;
        if (bottomOctave > 7)
          bottomOctave = 7;
        if (topOctave < 0)
          topOctave = 0;
        if (bottomOctave < 0)
          bottomOctave = 0;
        for (int i = 0; i < 32; i++)
          offColor(i, i < 16 ? topOctave : bottomOctave);
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
  for(uint16_t i=0; i<Y_DIM*X_DIM; i++) {
    trellis.setPixelColor(i, 0x000000);
    trellis.show();
    delay(50);
  }

  //activate all keys and set callbacks
  for(int i=0; i<Y_DIM*X_DIM; i++){
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING, true);
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_FALLING, true);
    trellis.registerCallback(i, blink);
  }

  
  for (byte i = 0; i < Y_DIM*X_DIM; i++)
    offColor(i, i < 16 ? topOctave : bottomOctave);
}


void loop() {
  if ( whiteButton.pressed() ) {
    if(state == 0){
      state = 1;
    } else {
      state = 0;
    }
  }

  trellis.read();
  
  if(state == 0){
  }
  
  greenButton.update();
  redButton.update();
  whiteButton.update();

  if ( greenButton.pressed() ) {  

  }
  if ( redButton.pressed() ) {
    
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
