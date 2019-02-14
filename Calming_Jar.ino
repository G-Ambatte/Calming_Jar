#include <Adafruit_NeoPixel.h>
#include <Math.h>

// NeoPixel data
// 60 LEDs, connected to pin 12
#define NUMPIXELS 60
#define LED_PIN 12

// Create the NeoPixel object
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Color values for main stage
#define _R_ 45
#define _G_ 0
#define _B_ 35

// Button data
#define BUTTON_PIN 2

// Debug mode
#define DEBUG

// Constants
const int delayTmr_MAX = 45;
const int delayTmr_MIN = 5;
const int colorRand_MAX = 10;

// Timers
unsigned long timer[NUMPIXELS] = {};
unsigned long start = millis();
unsigned long flashTimer;


// State Machine
// All possible states
enum StateMachine { OFF, INIT_CYCLE, CYCLE, INIT_SHUTDOWN, SHUTDOWN, INIT_OFF };
// Timer for the State Machine
unsigned long stateTimer;
// State durations
unsigned long SHUTDOWN_DURATION = 5000;
unsigned long SHUTDOWN_FLASH_DURATION = 1000;
// CYCLE lasts 25s in DEBUG, otherwise 2 minutes
#ifdef DEBUG
unsigned long CYCLE_DURATION = 25000;
#else
unsigned long CYCLE_DURATION = 120000;
#endif
// Flash state used during SHUTDOWN
bool shutdown_flash;

// Create the State Machine object
StateMachine state;


void setup() {

  // Read pin 0 and use that to seed the Random Number Generator
  randomSeed(analogRead(0));

  // Initialise NeoPixel strip
  strip.begin();

  // Set the State Machine to immediately start the cycle
  state = INIT_CYCLE;

  // Configure the button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // If in DEBUG, configure the Serial port for output
#ifdef DEBUG
  Serial.begin(9600);
#endif
}

void loop() {

  // Read current time
  unsigned long now = millis();

  // Run State Machine
  switch (state) {
    // If OFF, check for button press - if found, proceed to INIT_CYCLE
    case OFF:
      if (digitalRead(2) == LOW) {
        state = INIT_CYCLE;
      }
      break;
    // If INIT_CYCLE, record start time, set timer for CYCLE duration and change state to CYCLE
    case INIT_CYCLE:
      start = millis();
      state = CYCLE;
      stateTimer = now + CYCLE_DURATION;
    // If CYCLE, change color of LEDs and check for timer expiry
    // If timer expired, change state to INIT_SHUTDOWN
    case CYCLE:
      colorCycle();
      if ( now > stateTimer ) {
        state = INIT_SHUTDOWN;
      }
      break;
    // If INIT_SHUTDOWN, set timer for SHUTDOWN duration, set flag to flash red or white, and change state to SHUTDOWN
    case INIT_SHUTDOWN:
      state = SHUTDOWN;
      stateTimer = now + SHUTDOWN_DURATION;
      flashTimer = start;
      shutdown_flash = false;
    // If SHUTDOWN, change color of LEDs and check for timer expiry.
    // If timer expired, change state to INIT_OFF
    case SHUTDOWN:
      shutdownCycle();
      if ( now > stateTimer) {
        state = INIT_OFF;
      }
      break;
    // Clear pixels and change state to OFF
    case INIT_OFF:
      state = OFF;
      pixelClear();
      break;
    // In the event that the State Machine is somehow not in any of those states, set the State Machine to OFF.
    default:
      state = OFF;
      break;
  }
}

// This function makes the colors change during the main stage.
void colorCycle() {
  // Get the current time
  unsigned long now = millis();
  // For each NeoPixel
  for (int i = 0; i < NUMPIXELS; i++) {
    // Check if the Pixel's timer has expired
    if ( now > timer[i] ) {
      // If so, set a new timer
      timer[i] = millis() + random(delayTmr_MIN, delayTmr_MAX);
      // Determine the current brightness
      // using SINE(PI * percentage complete of CYCLE DURATION) to ramp up at beginning and down at the end
      // This means that by the time we are 16.67% through the CYCLE, the LEDs are at 50% max brightness
      // and remain at or above that level until we are 83.33% through the CYCLE
      // Could set a COLOR CYCLE DURATION different to the CYCLE_DURATION and have it loop multiple times
      float gamma = sin(PI * ((now - start) % CYCLE_DURATION) / CYCLE_DURATION);
      // Add some randomness to each LED; gives a flickering effect
      int randColor = random(0, colorRand_MAX) - (colorRand_MAX / 2);
      // Update the Pixel color
      pixelDraw(i, max(0, _R_ + randColor) * gamma, max(0, _G_ * randColor) * gamma, max(0, _B_ + randColor) * gamma, true);
      // If DEBUG, dump a bunch of info to the Serial port
#ifdef DEBUG
      Serial.print(i);
      Serial.print(": ");
      Serial.print(gamma);
      Serial.print(": ");
      Serial.print(max(0, _R_ + randColor));
      Serial.print("/");
      Serial.print(max(0, _G_ * randColor));
      Serial.print("/");
      Serial.print(max(0, _B_ + randColor));
      Serial.println("---");
#endif
    }
  }
}

// This function makes the colors change during the shutdown stage.
void shutdownCycle() {
  // Get the current time
  unsigned long now = millis();
  // Check if the flash timer has expired
  if ( now > flashTimer) {
    // If Debug, dump flash info to Serial port
#ifdef DEBUG
    Serial.println("Shutdown flash");
#endif
    // Set a new flash timer
    flashTimer = now + SHUTDOWN_FLASH_DURATION;
    // Create a flash color
    int flash_color;
    // If flash is ON
    if (shutdown_flash) {
      // Set flash color to 255
      // Because we use this to dynamically change Green and Blue while Red remains at 255
      // This will set the NeoPixel color to WHITE
      flash_color = 255;
    } else {
      // But if flash is OFF
      // Set flash color to 0
      // This will set the NeoPixel color to RED
      flash_color = 0;
    }
    // Change the state of the shutdown flash flag
    shutdown_flash = !shutdown_flash;
    // For each NeoPixel
    for (int i = 0; i < NUMPIXELS; i++) {
      // Change the color to the flash color (RED or WHITE) without live updating
      pixelDraw(i, 255, flash_color, flash_color, false);
    }
    // Only update once all Pixels have been changed
    pixelShow();
  }
}

// This function clears all NeoPixels
void pixelClear() {
  // For each NeoPixel
  for (int i = 0; i < NUMPIXELS; i++) {
    // Set the color to OFF without live udpating
    pixelDraw(i, 0, 0, 0, false);
  }
  // Only update once all Pixels have been changed
  pixelShow();
}

// This function handles changing the color of a single NeoPixel LED
void pixelDraw(int _i, int _r, int _g, int _b, bool _show) {
  // Set the color of Pixel i to the R, G, B values provided
  strip.setPixelColor(_i, strip.Color(_r, _g, _b));
  // If the Update flag has been set
  if (_show) {
    // Update the Pixel immediately
    pixelShow();
  }
}

// This function handles calling a NeoPixel output update
void pixelShow() {
  // Tell the strip to display the current color values
  strip.show();
}
