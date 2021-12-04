/* 16 LEDs Puzzle - see http://www.technoblogy.com/show?3PO0

   David Johnson-Davies - www.technoblogy.com - 4th December 2021
   ATtiny404 @ 20 MHz (internal oscillator; BOD disabled)
   
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

#include <avr/sleep.h>

// Globals
unsigned int Lights;
const unsigned long Timeout = 30000;                  // 30 seconds
volatile unsigned long Start;
static uint8_t DeadTime = 10;                         // Key debounce

// Display multiplexer **********************************************

void TimerSetup () {
  // Set up Timer/Counter TCB to multiplex the display
  TCB0.CCMP = (unsigned int)(F_CPU/250 - 1);          // Divide clock to give 250Hz
  TCB0.CTRLA = TCB_CLKSEL_CLKDIV1_gc | TCB_ENABLE_bm; // Enable timer, divide by 1
  TCB0.CTRLB = 0;                                     // Periodic Interrupt Mode
  TCB0.INTCTRL = TCB_CAPT_bm;                         // Enable interrupt
}

// Timer/Counter TCB interrupt - multiplexes the display and counts ticks
ISR(TCB0_INT_vect) {
  TCB0.INTFLAGS = TCB_CAPT_bm;                        // Clear the interrupt flag
  DoNextColumn();
}

/* To test buttons
void Puzzle (unsigned int keys) {
  Lights = Lights ^ keys;
}
*/

void Puzzle (unsigned int keys) {
  unsigned int ne, nw, se, sw;
  ne = nw = se = sw = keys;
  for (int i=0; i<3; i++) {
    ne = (ne & 0x7777)>>3; keys = keys | ne;
    nw = (nw & 0xEEEE)>>5; keys = keys | nw;
    se = (se & 0x7777)<<5; keys = keys | se;
    sw = (sw & 0xEEEE)<<3; keys = keys | sw;
  }
  Lights = Lights ^ keys;
}

void Randomize () {
  for (int i=0; i<16; i++) Puzzle(1<<(random(16)));
}
  
void DoNextColumn() {
  static unsigned int Keys, LastKeys = 0;
  static uint8_t Cycle;
  PORTB.OUT = 0x0F;                                   // Turn off previous LEDs
  Cycle = (Cycle + 1) & 0x03;
  uint8_t column = 1<<(Cycle+4);                      // Column 4 to 7
  PORTA.DIR = column;                                 // Make column output
 
  // Read keys in one column
  PORTA.OUT = 0;                                      // Make column low
  PORTB.DIR = 0;                                      // Make rows inputs
  Keys = Keys & ~(0x0F<<(Cycle*4)) | (~PORTB.IN & 0x0F)<<(Cycle*4);

  // Set lights in one column
  PORTA.OUT = column;                                 // Make column high
  uint8_t row = Lights>>(Cycle*4) & 0x0F;
  PORTB.DIR = row;                                    // Make active rows outputs
  PORTB.OUT = ~row;                                   // Copy row of lights to LEDs
  
  // Last cycle? Check Keys is stable
  if (Cycle == 3) {
    if (DeadTime != 0) {                              // Allow for key debounce
      DeadTime--;
    } else if (Keys != LastKeys) {                    // Change of state
      LastKeys = Keys;
      DeadTime = 10;
      if (Keys != 0) {
        Puzzle(Keys);
        Start = millis();                             // Delay sleep
      }
    }
  }
}

// Sleep **********************************************

void SleepSetup () {
  SLPCTRL.CTRLA |= SLPCTRL_SMODE_PDOWN_gc;
  SLPCTRL.CTRLA |= SLPCTRL_SEN_bm;
  // Turn on pullups on unused pins PA0 to PA3 for minimal power in sleep
  PORTA.PIN0CTRL = PORT_PULLUPEN_bm;
  PORTA.PIN1CTRL = PORT_PULLUPEN_bm;
  PORTA.PIN2CTRL = PORT_PULLUPEN_bm;
  PORTA.PIN3CTRL = PORT_PULLUPEN_bm;
}

// Pin change interrupt is just used to wake us up
ISR (PORTB_PORT_vect) {
  PORTB.INTFLAGS = 0x0F;                              // Clear interrupt flags
}

// Setup **********************************************

// Set input state of PB0 to PB3
void SetRows (uint8_t defn) {
  PORTB.PIN0CTRL = defn;
  PORTB.PIN1CTRL = defn;
  PORTB.PIN2CTRL = defn;
  PORTB.PIN3CTRL = defn;
}

void setup () {
  SleepSetup();
  TimerSetup();
  Randomize();
}

void loop () {
  PORTA.DIR = 0x00;                                   // Make columns inputs
  SetRows(PORT_PULLUPEN_bm);                          // Turn on input pullups
  Start = millis();
  while (millis() - Start < Timeout) {                // Stay awake until timeout
    if (Lights == 0xFFFF) {                           // Solved puzzle?
      delay(1000);
      for (int n=0; n<4; n++) {
        Lights = ~Lights;
        delay(500);
      }
      Randomize();
    }
  }

  // Get ready to go to sleep
  PORTA.DIR = 0xF0;                                   // Make columns outputs
  PORTA.OUT = 0x00;                                   // Make columns low
  PORTB.DIR = 0;                                      // Make rows inputs
  SetRows(PORT_PULLUPEN_bm | PORT_ISC_LEVEL_gc);      // Add pin change interrupts
  sleep_cpu();                                        // Go to sleep
  DeadTime = 100;
}
