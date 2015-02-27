/* Nerf gun speed meter using atmega8 and photodiodes
 * Pinout:
 * PB0 - Input Capture (OR'd led sensors, active high)
 * PD0-PD6 - 7-segment display cathodes pin a-f (active low)
 * PB1-PB3 - 7-segment display anode driver (BC560 or similar)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdbool.h>

#define MAX_SPEED 37500 //125000; // distance[dm] * cpu_frequency[Hz] / timer1_prescaler = (here) 1dm * 8000000 / 64
#define MAX_OVERFLOWS (MAX_SPEED/65536+1) //maximum count of overflows for calculation
#define PLEXDELAY 32 //~244Hz = 8MHz/1024/plexdelay

unsigned char currentSegment = 0; //currently multiplexed segment (equals decade)
unsigned char currentCode[3] = {0, 0, 0}; //segment code to display on every segment
const unsigned char segmentCode[] = {
  //0gfedcba, active-low
  0b11000000, //0 or D
  0b11111001, //1 or I
  0b10100100, //2
  0b10110000, //3
  0b10011001, //4
  0b10010010, //5 or S
  0b10000010, //6 or G
  0b11111000, //7
  0b10000000, //8 or B
  0b10010000, //9
  0b11111111, //segment off
  0b10001000, //'A'
  0b11000110, //'C'
  0b10000110, //'E'
  0b10001110, //'F'
  0b10001001, //'H'
  0b11110001, //'J'
  0b11000111, //'L'
  0b10001100, //'P'
  0b11000001, //'U'
  0b10111111, //'-'
  0b11110111  //'_'
};
#define SEG_OFF  10
#define LETTER_A 11
#define LETTER_C 12
#define LETTER_E 13
#define LETTER_F 14
#define LETTER_H 15
#define LETTER_J 16
#define LETTER_L 17
#define LETTER_P 18
#define LETTER_U 19
#define SYM_DASH 20
#define SYM_USCR 21
#define SETTEXT(a,b,c) (setSegments(-1,a,b,c))
#define SETNUM(n) (setSegments(n,LETTER_E,0,1)) //display a number, if n is -1, display "E01"

unsigned int overflows = 0, captured = 0;
unsigned int capture[2] = {0, 0};

//sets the 3 segment displays to number "num" including separation into single digits
//if num is < 0, code2..0 are displayed directly
void setSegments(int num, unsigned char code0, unsigned char code1, unsigned char code2) {
  if( num < 0 ) { //display text
    currentCode[0] = segmentCode[code0];
    currentCode[1] = segmentCode[code1];
    currentCode[2] = segmentCode[code2];
  } else { //display number
    unsigned char digit;
    while( num > 999 )
      num -= 1000;
    for( digit = 0; num >= 100; digit++ )
      num -= 100;
    currentCode[0] = segmentCode[digit];
    for( digit = 0; num >= 10; digit++ )
      num -= 10;
    currentCode[1] = segmentCode[digit];
    currentCode[2] = segmentCode[num];
  }
}

//timer1 input capture event
ISR(TIMER1_CAPT_vect) {
  if( captured < 2 ) { //just capture two values
    if( captured == 0 )
      overflows = 0; //reset overflow counter if first value was captured
    capture[captured] = ICR1;
    captured++;
  }
}

//timer1 overflow
//a capture will be dropped if too many overflows occured before the second capture
ISR(TIMER1_OVF_vect) {
  if( captured == 1 ) {
    overflows++;
    if( overflows > MAX_OVERFLOWS )
      captured = 0;
  }
}

//7-segment mux routine
ISR(TIMER2_COMP_vect) {
  PORTB |= 0b00001110; //disable all segments
  PORTD = currentCode[currentSegment]; //set current digit
  PORTB &= ~(1 << (currentSegment+1));
  if( currentSegment == 2 )
    currentSegment = 0;
  else
    currentSegment++;
  TCNT2 = 0; //not needed due to CTC (clear timer on compare) mode
}

//calculate the speed using captured values (and overflow count)
// v = maxSpeed/(capture[1]+65536*overflow-capture[2])
// returns speed in dm/s or -1 in case of failure
int calculateSpeed() {
  unsigned long numerator = MAX_SPEED;
  const unsigned long halfNumerator = MAX_SPEED/2; //for rounding
  unsigned long denominator = capture[1];
  denominator += (unsigned long)overflows*65536;
  denominator -= capture[0];
  if( denominator <= numerator/999 ) //denominator far too small! display error
    return -1;
  int result = 0;
  while( numerator >= denominator ) {
    result++;
    numerator -= denominator;
  }
  if( numerator >= halfNumerator )
    result++; //correct rounding
  return result;
}

int main() {
  DDRB = 0b00001110; //led drivers + PB0 input capture
  DDRD = 0b01111111; //7-segment cathodes
  PORTB = 0b11111110; //enable pullups on every pin except PB0 input capture, disable segment anodes
  PORTD = 0b11111111; //disable all leds, set pullup on PD7

  //Timer2 for 7-segment multiplexing
  OCR2 = PLEXDELAY; //set compare match value
  TCCR2 = 0b01000111; //set CTC mode, prescaler 1024

  //Timer1 for speed measurement
  TCCR1B = 0b11000011; //input capture noise canceler + capture on rising edge, prescaler 64

  //Timer1+Timer2 interrupts: enable input capture & overflow
  TIMSK = 0b10100100;

  SETTEXT(LETTER_H,1,SEG_OFF);
  sei();

  while(1) {
    sleep_mode();
    if( overflows > MAX_OVERFLOWS )
      SETTEXT(LETTER_L,0,SEG_OFF); //display "LO " if speed was too low
    if( captured == 1 )
      SETTEXT(SYM_DASH,SYM_DASH,SYM_DASH); //display dashes when result is pending
    if( captured == 2 ) {
      SETNUM(calculateSpeed());
      overflows = 0;
      captured = 0;
    }
  }

  return 0;
}
