#include <avr/io.h>
#include <avr/interrupt.h>

extern "C" {
    #include "glcd/glcd.h"
    #include "glcd/fonts/Liberation_Sans15x21_Numbers.h"
    #include "glcd/fonts/font5x7.h"
    #include "glcd/devices/AVR8.h"
    #include "glcd/unit_tests.h"
}

volatile unsigned int time=0;

int main(void)
{
    glcd_init();

    glcd_set_font(Font5x7,5,7,32,127);
    glcd_clear_buffer();
    glcd_draw_string_xy(0,0,"Hello World!");
    glcd_write();

    while(1);

    // OCR2 = 255;
    // TCCR2 |= (1 << WGM21);
    // // Set to CTC Mode
    // TIMSK |= (1 << OCIE2);
    // //Set interrupt on compare match
    // TCCR2 |= (1 << CS21);
    // // set prescaler to 64 and starts PWM
    // sei();
    // // enable interrupts
    // DDRC |= (1 << 0);
    // PORTC &= ~(1 << 0);    
    // while (1) {
    //     if(time % 60000 > 30000) {
    //         PORTC &= ~(1 << 0);
    //     } else {
    //         PORTC |= (1 << 0);
    //     }
    // }
}
 
// ISR (TIMER2_COMP_vect)
// {
//     // action to be done every 250us 
//     time++;
//     if(time == 60000) {
//         time = 0;
//     }
//     //glcd_test_hello_world();
// }
