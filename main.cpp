#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

extern "C" {
    #include "n5110/n5110.h"
    // #include "glcd/fonts/Liberation_Sans15x21_Numbers.h"
    // #include "glcd/devices/AVR8.h"
}

volatile unsigned int time=0;

int main(void)
{
    //�������������� �������
    Lcd_init();
    LcdContrast(65);
    DDRB |= (1 << 0);
    PORTB |= (1 << 0);
    _delay_ms(50);
    while(1)
    {
        //�������� ��������� 
        //������� �������
        Lcd_clear();
        //Lcd_prints ��� ������ ��������� ������
        Lcd_prints(0, 0, FONT_1X,(unsigned char *)PSTR("Hello!"));
        //��������� ������  
        Lcd_update();
        //�������� 2 �������
        _delay_ms(2000);
        //������� �������
        Lcd_clear();
        //�������� ������
        for (int i=0;i<=10;i++)
        {
            Lcd_circle(40, 24, i, PIXEL_ON);
        }
        //��������� ������
        Lcd_update();
        //�������� 2 �������
        _delay_ms(2000);
    }
    /*DDRB = 0xFF;
    while(true) { 
        PORTB |= (1 << 5);
        delay_ms(200);
        PORTB &= ~(1 << 5);
        delay_ms(200);
    }*/

    // glcd_init();
    // glcd_set_contrast(55);
    // glcd_set_font(Liberation_Sans15x21_Numbers,15,21,46,57);
    // while(true) { 
    //     GLCD_SCK_LOW();
    //     GLCD_DIN_LOW();
    //     GLCD_DC_HIGH();
    //     GLCD_RESET_HIGH();
    //     glcd_clear_buffer();
    //     glcd_draw_string_xy(0,0,"test");
    //     glcd_write();
    //     delay_ms(80);
    // }
/*    OCR2 = 255;
    // Set to CTC Mode
    TCCR2 |= (1 << WGM21);
    //Set interrupt on compare match
    TIMSK |= (1 << OCIE2);
    // set prescaler to 64 and starts PWM
    TCCR2 |= (1 << CS21);
    // enable interrupts
    sei();
    DDRC |= (1 << 0);
    PORTC &= ~(1 << 0);    
    while (1) {
        if(time % 60000 > 30000) {
            PORTC &= ~(1 << 0);
        } else {
            PORTC |= (1 << 0);
        }
    }*/
}
 
/*ISR (TIMER2_COMP_vect)
{
    time++;
    if(time == 60000) {
        time = 0;
    }
}*/
