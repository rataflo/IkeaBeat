#include <Adafruit_NeoPixel.h>



/*  
 *   Captor for heart beat. Transmit each beat to the jellyfish via ir led.
 *   
 *   Use : 
 *   MAX30102 sensor.
 *   Arduino nano:
 *    MAX30102 SDA -> A4 Arduino
 *    MAX30102 SCL -> A5 Arduino
 *    2 x 4,7k resistor between SDA<->VIN and SCL<->VIN
 *    MAX30102 VIN -> 5V Arduino
 *    MAX30102 GND -> GND Arduino
 *    
 *   IR Led
 *   
 *   The heart beat detection algorithm use raise and big drop to detect a beat. This method is good to manage different finger pressure on the captor.
 *   
 *   Florent Galès 2019
 *   Licence Rien à brancler \ Do what the fuck you want licence.
*/

/*  
 *   Heart In A Box
 *   Attiny85, Pulse sensor captor (MAX30120), Red led and 3d printed heart.
 *   Author: Flo Gales
 *   License: Rien à branler / Do What The Fuck You Want
*/

#include "MAX30105.h" // Sparkfun library.

// Constants.
//#define DEBUG // Uncomment to activate debug.
#define DEFAULT_MILLIS 750 // Defaut value for time between 2 beat. 800 = 75 bpm, 750 = 80bpm
#define DEFAULT_BPM 80 // Defaut bpm to start. 
#define MIN_THRESHOLD 90000 // Min value to begin process.
#define MIN_HEART_INTERVAL 400 // Min and max heart beat interval in millis. Not inteded for yogi or people running.
#define MAX_HEART_INTERVAL 1200
#define DROP_HEART_VALUE 180 // Basically to find a beat we check a big drop after a raise. With this method we can check a beat in every pressure situation on the sensor.
    
MAX30105 heartSensor;

unsigned long maxHeart = 0;
unsigned long lastBeat = 0;
unsigned long lastRead = 0;
unsigned long lastCheckHeart = 0;
unsigned int tabMoyenne[] = {DEFAULT_MILLIS, DEFAULT_MILLIS, DEFAULT_MILLIS, DEFAULT_MILLIS, DEFAULT_MILLIS};
float bpm = DEFAULT_BPM;
float average = 0;
byte nbEchantillon = 0;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(42, 6, NEO_GRB + NEO_KHZ800);

unsigned long lastUpdatePixel = 0;
byte currPixel = 0;

void setup(){
  //pinMode(A9, INPUT);
  pixels.begin(); 
  for(int i=0;i<42;i++){
    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    pixels.setPixelColor(i, pixels.Color(0,0,0)); // Moderately bright green color.
    pixels.show(); // This sends the updated pixel color to the hardware.
  }
  
  #ifdef DEBUG
    Serial.begin(115200);
  #endif  
  
  if (!heartSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    #ifdef DEBUG
      Serial.println("MAX30105 was not found. Please check wiring/power. ");
    #endif  
    while (1);
  }
  #ifdef DEBUG
    Serial.println("Place your index finger on the sensor with steady pressure.");
  #endif 
  
  heartSensor.setup(); //Configure sensor with default settings
  heartSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  heartSensor.setPulseAmplitudeGreen(0); //Turn off Green LED
  
  initMeasures();
}

void loop(){
  
  unsigned long currentMillis = millis();
  unsigned long heart = heartSensor.getIR();
  
  // If a finger on the sensor.
  if(heart >= MIN_THRESHOLD){
    int speedPix = average / 18;
    
    if(currentMillis > lastUpdatePixel + speedPix){
      lastUpdatePixel = currentMillis;
      if(currPixel < 18){
        pixels.setPixelColor(currPixel, 255, 0, 0);
        pixels.show(); 
        pixels.setPixelColor(currPixel, 0, 0, 0); // switch off for next loop.
      } else { // flash the whole hearth part.
        for(int i = 18; i < 45; i++){
          pixels.setPixelColor(i, 255, 0, 0);
        }
        pixels.show();
        for(int i = 18; i < 45; i++){// switch off for next loop.
          pixels.setPixelColor(i, 0, 0, 0);
        }
        // may be slow down next display???
      }
      currPixel = currPixel + 1 > 18 ? 0 : currPixel + 1;
    }
    
  }else { // react to sound.
    int n   = analogRead(A9);                        // Raw reading from mic 
    n   = abs(n - 512); // Center on zero
    n = n > 10 ? n * 5 : 0;
    for(int i=17;i<42;i++){
      pixels.setPixelColor(i, pixels.Color(n*255/255,0,0)); // red
    }
    pixels.show();
  }
  checkHeart(currentMillis, heart);
}

void checkHeart(unsigned long currentMillis, unsigned long heart){
  
  #ifdef DEBUG
    /*Serial.print(heart);
    Serial.print(" ");
    Serial.print(bpm);
    Serial.println(" ");*/
  #endif
  
  if(heart >= MIN_THRESHOLD){
    lastRead = 0;
    //Détection du maxiumum
    if(heart > maxHeart){
      maxHeart = heart;
    }else { // Max reached we test a big drop
      if(heart < maxHeart - DROP_HEART_VALUE){
        if(lastBeat != 0){ // Si on as déja commencé à tracer l'intervalle entre 2 pics.
          float intervalle = (currentMillis - lastBeat);
          
          if(intervalle < MAX_HEART_INTERVAL && intervalle > MIN_HEART_INTERVAL){ // We avoid exotic interval.
            fifoMeasures(intervalle);
            bpm = (1.0/average) * 60000;
          }
          maxHeart = MIN_THRESHOLD;
        }
        lastBeat = currentMillis;
      }
    }
  } else { // Check for 1 second without finger on the sensor to reset values;
    
    lastRead = lastRead == 0 ? currentMillis : lastRead;
    if(currentMillis > (lastRead  + 1000)){
      initMeasures();
      lastRead = currentMillis;
    }
  }
}



float fifoMeasures(float newIntervalle){
  // When we have 5 measure we avoid big gap with average.
  if(nbEchantillon < 5 || (newIntervalle < average + 100 && newIntervalle > average - 100)){
    float totIntervalle = newIntervalle;
    for(int i = 3; i >= 0; i--){
      tabMoyenne[i+1] = tabMoyenne[i];
      totIntervalle += tabMoyenne[i+1];
    }
    tabMoyenne[0] = newIntervalle;
    average = totIntervalle / 5;
    nbEchantillon = nbEchantillon < 5 ? nbEchantillon++ : 0; 
  }
  return average;
}

void initMeasures(){
  
  for(int i = 0; i < 5; i++){
    tabMoyenne[i] = DEFAULT_MILLIS;
  }
  average = DEFAULT_MILLIS;
  bpm = DEFAULT_BPM;
  lastBeat = 0;
  nbEchantillon = 0;
  maxHeart = 0;
}
