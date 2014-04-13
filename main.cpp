#include <avr/io.h>
#include <avr/interrupt.h>
#include "glcd.h"

volatile unsigned int time=0;

int main(void)
{
    // glcd_init();
    OCR2 = 255;
    TCCR2 |= (1 << WGM21);
    // Set to CTC Mode
    TIMSK |= (1 << OCIE2);
    //Set interrupt on compare match
    TCCR2 |= (1 << CS21);
    // set prescaler to 64 and starts PWM
    sei();
    // enable interrupts
    DDRC |= (1 << 0);
    PORTC &= ~(1 << 0);    
    while (1) {
        if(time % 60000 > 30000) {
            PORTC &= ~(1 << 0);
        } else {
            PORTC |= (1 << 0);
        }
    }
}
 
ISR (TIMER2_COMP_vect)
{
    // action to be done every 250us 
    time++;
    if(time == 60000) {
        time = 0;
    }
    //glcd_test_hello_world();
}
