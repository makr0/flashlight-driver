//
// XM-L Driver based on
// LuxDrv 0.30    by DrJones 2011
//
// License: Free for private and noncommercial use for members of BudgetLightForum
//
// i changed DrJones version quite a bit
// new features:
// slow turn-on for all PWM modes (only noticeable in high-mode)
// stobe mode is 'hidden' behind battmon-mode
// removed ramping and timer
//
// TODO: make strobe frequency adjustable and store it

#define outpin PB1
#define PWM OCR0B  //PWM-value

#define adcpin 2
#define adcchn 1

#define portinit() do{ DDRB=(1<<outpin); PORTB=0xff-(1<<outpin)-(1<<adcpin);  }while(0)

#include <avr/pgmspace.h>
#define byte uint8_t
#define word uint16_t


//########################################################################################### MODE/PWM SETUP 

//251, 252 are special mode codes handled differently, all other are PWM values
// dummy, will be copied over
byte modes[10];
byte size_modes;
//PROGMEM byte modes1[]  = { 0 };
//PROGMEM byte modes2[]  = { 0 };
//PROGMEM byte modes3[]  = { 0 };

PROGMEM byte modes1[]  = { 0,      13,50,255,  252             }; // used if no jumper set
PROGMEM byte modes2[]  = { 0,      14,50,255                   }; // used if PB0 low (jumper 2)
PROGMEM byte modes3[]  = { 0,      255,50,14                   }; // used if PB4 low (jumper 3)
//                         dummy   main modes, battmode and strobe
#define LOCKTIME 12 //time in 1/8 s until a mode gets locked.  12/8=1.5s
#define BATTMON  125//enable battery monitoring with this threshold
//#define BATTMON  211 // for testing...

//###########################################################################################

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>



#define WDTIME 0b01000011  //125ms

#define sleepinit() do{ WDTCR=WDTIME; sei(); MCUCR=(MCUCR &~0b00111000)|0b00100000; }while(0) //WDT-int and Idle-Sleep

#define SLEEP asm volatile ("SLEEP")

#define pwminit() do{ TCCR0A=0b00100001; TCCR0B=0b00000001; }while(0)  //chan A, phasePWM, clk/1  ->2.35kHz@1.2MHz

#define adcinit() do{ ADMUX =0b01100000|adcchn; ADCSRA=0b11000100; }while(0) //ref1.1V, left-adjust, ADC1/PB2; enable, start, clk/16
#define adcread() do{ ADCSRA|=64; while (ADCSRA&64); }while(0)
#define adcresult ADCH


#define ADCoff ADCSRA&=~(1<<7) //ADC off (enable=0);
#define ADCon  ADCSRA|=(1<<7)  //ADC on
#define ACoff  ACSR|=(1<<7)    //AC off (disable=1)
#define ACon   ACSR&=~(1<<7)   //AC on  (disable=0)


//_____________________________________________________________________________________________________________________


volatile byte mypwm=0;
volatile byte ticks=0;
volatile byte mode=1;
volatile byte strobedelay = 1;
volatile byte strobedir = 1;
byte pmode=50;


byte eep[32];  //EEPROM buffer
byte eepos=0;

byte lowbattcounter=0;

/*
 * initialise EEPROM
 * This will be used to build the initial eeprom image.
 */
const uint8_t EEMEM eeprom[64] =
{   0x87, 0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF,
    0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF,
    0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF,
    0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF};

void eepsave(byte data) {  //central method for writing (with wear leveling)
  byte oldpos=eepos;
  eepos=(eepos+1)&15;  //wear leveling, use next cell
  EEARL=eepos; EEDR=data; EECR=32+4; EECR=32+4+2;  //WRITE  //32:write only (no erase)  4:enable  2:go
  while(EECR & 2); //wait for completion
  EEARL=oldpos;           EECR=16+4; EECR=16+4+2;  //ERASE  //16:erase only (no write)  4:enable  2:go
}
void eepsave_raw(byte addr, byte data) {  //method for writing at addr without wear leveling
  EEARL=addr; EEDR=data; EECR=32+4; EECR=32+4+2;  //WRITE  //32:write only (no erase)  4:enable  2:go
  while(EECR & 2); //wait for completion
}


int jumper2_set(void) {
    return (PINB & (1 << PB0)) == 0;
}
int jumper3_set(void) {
    return (PINB & (1 << PB4)) == 0;
}

inline void getmode(void) {  //read current mode from EEPROM and write next mode

  eeprom_read_block(&eep, 0, 32);  //+44                //read block 
  while((eep[eepos]==0xff) && (eepos<32)) eepos++; //+16   //find mode byte
  if (eepos<32) mode=eep[eepos];
  else eepos=0;   //+6  //not found

  byte next=0;

  if (mode & 0x80) {  //last on-time was short
    mode&=0x7f;   if (mode>=size_modes) mode=1;
    next=mode+1;  if (next>=size_modes) next=1;
  } else {
      if( jumper2_set() || jumper3_set() ) {
          next=1;
          if (mode==1) next=2;
      }  // next mode is same as prev
      else { next=1; }  //previous mode was locked, this one is yet a short-on, so set to restart from 1st mode.
  }

  eepsave(next|0x80); //write next mode, with short-on marker
}

ISR(WDT_vect) {  //WatchDogTimer interrupt
    if (ticks<255) ticks++;
    if (ticks==LOCKTIME){
        if( jumper3_set() ) {
            eepsave( 1 ); // always start in first mode if jumper 3 set
        } else {
            eepsave(mode);  //lock mode after 1.5s
        }
    }

    #ifdef BATTMON //code to check voltage and ramp down
      adcread();
      eepsave_raw(63,adcresult);
      if (adcresult<BATTMON) { if (++lowbattcounter>8) {PWM=(PWM>>1)+3;lowbattcounter=0;} }
      else lowbattcounter=0;
    #endif
}

int main(void) {
    byte i=0;
  if( jumper2_set() ) {
      for(i=0;i++< sizeof(modes2);) {
          modes[i]=pgm_read_byte(&modes2[i]);
      }
      size_modes = sizeof(modes2);
  } else {
      if( jumper3_set() ) {
          for(i=0;i++< sizeof(modes3);) {
              modes[i]=pgm_read_byte(&modes3[i]);
          }
          size_modes = sizeof(modes3);
      } else {
          for(i=0;i++< sizeof(modes1);) {
              modes[i]=pgm_read_byte(&modes1[i]);
          }
          size_modes = sizeof(modes1);
      }
  }

  portinit();
  sleepinit();
  ACoff;
#ifdef BATTMON
  adcinit();
#else
  ADCoff;
#endif
  pwminit();


  getmode();  //get current mode number from EEPROM

  pmode=modes[mode];  //get actual PWM value (or special mode code)


  switch(pmode){

    // strobe
    case 251:  mypwm=255;
                while(1){
                    PWM=mypwm;
                    _delay_ms(30);
                    PWM=0;
                    _delay_ms(300);
                } break;

    // battmon, then strobe
    case 252:  
               #ifdef BATTMON    
                  adcread();
                  i=adcresult;
                  PWM=255;
                  _delay_ms(20);
                  PWM=0;
                  _delay_ms(2000);
                  while (i>BATTMON) {
                      PWM=80;
                      _delay_ms(40);
                      PWM=0;
                      _delay_ms(700);
                      i-=5;
                  }
                  _delay_ms(1000);
                  PWM=255;
                  _delay_ms(20);
                  PWM=0;
                  _delay_ms(1500);
               #endif
               mypwm=255;
               strobedelay=5;
               strobedir=1;
               while(1){
                   PWM=mypwm;
                   _delay_ms(30);
                   PWM=0;
                   i=0;
                   while (i++ < strobedelay ) {
                       _delay_ms(30);
                   }
//                   if( strobedelay == 0 ) strobedir = 1;
//                   if( strobedelay == 20 ) strobedir = -1;
//                   strobedelay+=strobedir;
               } break;  //beacon 10s //+48


    default:   mypwm=0;
              // ramp up to value
               while( mypwm < pmode ) {
                   mypwm++;
                   PWM=mypwm;
                   _delay_ms(7);
               }
               while(1){PWM=pmode;SLEEP;}  //all other: use as PWM value
               break;

   //mypwm: prepared for being ramped down in WDT on low-batt condition.

  }//switch
  return 0;
}//main
