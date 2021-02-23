/* Neotrellis + Teensy Midi controller 

  Author: Andreas Sandersen
  2021

*/

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
 * - View midi sequence. Last pressed note from View 0 as input at steps
 */

#include "Adafruit_NeoTrellis.h"
#include "FifteenStep.h"
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
  
  { Adafruit_NeoTrellis(0x30), Adafruit_NeoTrellis(0x2F) }
  
};
//pass this matrix to the multitrellis object
Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM/4, X_DIM/4);

bool seqView = false;
// Keyboard
int semitones[16] = {
    0,  2, 4,  5, 7, 9, 11, -3,
    -1, 1, 3, -1, 6, 8, 10, -2,
};
byte octaveBaseNotes[] = {
    24, 36, 48, 60, 72, 84, 96, 108,
};
int topOctave = 3, bottomOctave = 1;

int midiKey = 0;
//Sequencer
int clockCount;
int selectedNote = 0;
bool playing = false;
int sequence[Y_DIM*X_DIM];
int sequenceCount;
int steps = 32;
int tempo = 130;


//Translate neotrellis keys
int translateKey(int key){
    int newKey;
    switch (key) {
      case 0:
        newKey = 3;
        break;
      case 1:
        newKey = 2;
        break;
      case 2:
        newKey = 1;
        break;
      case 3:
        newKey = 0;
        break;
      case 4:
        newKey = 7;
        break;
      case 5:
        newKey = 6;
        break;
      case 6:
        newKey = 5;
        break;
      case 7:
        newKey = 4;
        break;
      case 8:
        newKey = 11;
        break;
      case 9:
        newKey = 10;
        break;
      case 10:
        newKey = 9;
        break;
      case 11:
        newKey = 8;
        break;
      case 12:
        newKey = 15;
        break;
      case 13:
        newKey = 14;
        break;
      case 14:
        newKey = 13;
        break;
      case 15:
        newKey = 12;
        break;
      case 16:
        newKey = 19;
        break;
      case 17:
        newKey = 18;
        break;
      case 18:
        newKey = 17;
        break;
      case 19:
        newKey = 16;
        break;
      case 20:
        newKey = 23;
        break;
      case 21:
        newKey = 22;
        break;
      case 22:
        newKey = 21;
        break;
      case 23:
        newKey = 20;
        break;
      case 24:
        newKey = 27;
        break;
      case 25:
        newKey = 26;
        break;
      case 26:
        newKey = 25;
        break;
      case 27:
        newKey = 24;
        break;
      case 28:
        newKey = 31;
        break;
      case 29:
        newKey = 30;
        break;
      case 30:
        newKey = 29;
        break;
      case 31:
        newKey = 28;
        break;
    }
    return (newKey);
}

int getOctave(int key){
  int octave;
  if (key > 15) {
    octave = topOctave;
  } else {
    octave = bottomOctave;
  }
  return (octave);
}

void offColor(int key, int octave) {
  int trellisKey = translateKey(key);
  octave = getOctave(key);
  
  if (semitones[key % 16] < 0) {
    trellis.setPixelColor(trellisKey, 0);
  } else {
    trellis.setPixelColor(trellisKey, Wheel(octaveBaseNotes[octave]));
  }
}

TrellisCallback blink(keyEvent evt){
  int trellisKey = evt.bit.NUM;
  midiKey = translateKey(trellisKey);
  int octave = getOctave(midiKey);
  
  if(!seqView){
      if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
        trellis.setPixelColor(trellisKey, Wheel(random(255)));
      } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
        offColor(midiKey, octave);
      }
  
      if (semitones[midiKey % 16] >= 0) {
        if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
          usbMIDI.sendNoteOn(semitones[midiKey % 16] + octaveBaseNotes[octave], 64, MIDI_CHANNEL);
          Serial.printf("Note On %i\n", semitones[midiKey % 16] + octaveBaseNotes[octave]);
          Serial.printf("Button number %i\n", midiKey);
          selectedNote = semitones[midiKey % 16] + octaveBaseNotes[octave];
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
  
          for (int i = 0; i < Y_DIM*X_DIM; i++){
            int key = translateKey(i);
            offColor(key, key > 15 ? topOctave : bottomOctave);
          }
        }
      }
  } else if (seqView){
    if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
      Serial.printf("Sequencer Note  %i\n", selectedNote);
      if (sequence[midiKey] == 0){
        sequence[midiKey] = 1;
        trellis.setPixelColor(trellisKey, Wheel(random(255)));
      } else{
        sequence[midiKey] = 0;
        trellis.setPixelColor(trellisKey, 0);
      }
    }
  }
  trellis.show();
  return 0;
}

void setup() {
  Serial.begin(115200);
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
    int key = translateKey(i);
    trellis.setPixelColor(key, Wheel(map(i, 0, Y_DIM*X_DIM, 0, 255)));
    trellis.show();
    delay(50);
  }
  //Fill sequence with 0s (inactive steps)
  for ( int i = 0; i < Y_DIM*X_DIM; ++i ){
    sequence[i] = 0;
  }
  //activate all keys and set callbacks
  for(int i=0; i<Y_DIM*X_DIM; i++){
    updateKeys();
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING, true);
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_FALLING, true);
    trellis.registerCallback(i, blink);
  }

  usbMIDI.setHandleSongPosition(onSongPosition);
  usbMIDI.setHandleClock(onClock);
  usbMIDI.setHandleStart(onStart);
}


void loop() {
  if (whiteButton.pressed()) {
    Serial.printf("white button pressed \n");
    Serial.printf("Seqview: \n", seqView);
    if (!seqView ){
      seqView = true;
    } else{
      seqView = false;
    }
    updateKeys();
  }
  if (greenButton.pressed()) {  
    usbMIDI.sendRealTime(usbMIDI.Start);
  }
  if (redButton.pressed()) {
    usbMIDI.sendRealTime(usbMIDI.Stop);    
    for (int i = 23; i < 119; i++){
      usbMIDI.sendNoteOff(i, 64, MIDI_CHANNEL);
    } 
  }
  greenButton.update();
  redButton.update();
  whiteButton.update();

  if(!digitalRead(INT_PIN)){
    trellis.read();
  }
  
  while (usbMIDI.read()) {
    // ignore incoming messages
  }
}

void updateKeys(){
  for(int i=0; i<Y_DIM*X_DIM; i++){
    int key = translateKey(i);
    if(seqView){
       if(sequence[i] == 1){
        trellis.setPixelColor(key, Wheel(random(255)));
       } else{
        trellis.setPixelColor(key, 0);
       }
    } else{
      offColor(i, i < 16 ? topOctave : bottomOctave);
    }
  } 
  trellis.show();
}

void onClock() {
  Serial.printf("Onclock %i\n");
  sequenceCount++;
  if (clockCount<=3){
    if(sequenceCount > 31){
      sequenceCount = -1;
    }
    if(seqView){
      int key = translateKey(sequenceCount);
      trellis.setPixelColor(key, 0xFF0000);  
      trellis.show();
      if(sequence[sequenceCount] == 1){
        usbMIDI.sendNoteOn(selectedNote, 64, MIDI_CHANNEL);  
      } else{
        usbMIDI.sendNoteOff(selectedNote, 64, MIDI_CHANNEL);  
      }  
    }
  }
  clockCount++;
  clockCount %= 24;
}

void onStart(){
  clockCount = 0;
  sequenceCount = -1;
}

void onSongPosition(uint16_t semiQ){
  clockCount = semiQ*6 ; 
  Serial.printf("songpos %i\n");
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
