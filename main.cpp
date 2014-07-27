#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

extern "C" {
    #include "n5110/n5110.h"
    #include "dht/dht.h"
    // #include "glcd/fonts/Liberation_Sans15x21_Numbers.h"
    // #include "glcd/devices/AVR8.h"
}

volatile unsigned int time=0;

char *itoa(int a, int len=0) {
    static char buf[16];
    char *p = buf + 15;
    p[0] = 0;
    int v = a;
    int l = len == 0?15:len;
    int i;
    for(i=0;i<l;i++) {
        p--;
        p[0] = '0'  + (v % 10);
        v = v / 10;
    }
    return p;
}

void backlit_on() {
    DDRB |= (1 << 0);
    PORTB |= (1 << 0);
}

void backlit_off() {
    DDRB |= (1 << 0);
    PORTB &= ~(1 << 0);
}

//#define DHT_PIN(reg) BIT(C, 0, reg)
#define DHT_PIN 1

static inline int16_t dhtproc(dht_request_t req, uint16_t arg){
    switch(req){
        case DHT_READ_PIN: 
            DDRC &= ~(1 << DHT_PIN);
            return (PINC & (1 << DHT_PIN)) > 0?1:0; 
        case DHT_WRITE_PIN:
            DDRC |= (1 << DHT_PIN);
            if(arg) {
                PORTC |= (1 << DHT_PIN);
            } else {
                PORTC &= ~(1 << DHT_PIN);
            }
            return 0;
        case DHT_DELAY_MS:
            while(arg--) {
                _delay_ms(1);
            }
            break;
        case DHT_DELAY_US:
            while(arg--) {
                _delay_us(1);
            }
            break;
        default:
            return -1;
    }
    return 0;
}

dht_t dht; 

void init_dht() {
    PORTC &= ~(1 << DHT_PIN);
    //PORTC |= (1 << DHT_PIN);
    DHT_Init(&dht, dhtproc);
}

int temperature = 0;
int humidity = 0;

void update_dht() {
    if(DHT_Read11(&dht) == DHTLIB_OK){
        temperature = (int)dht.temperature;
        humidity = (int)dht.humidity;
    } else {
        temperature = -DHT_Read11(&dht);
        humidity = 99;
        _delay_ms(1000);
    }
}

void printInt(int a, int n=4, int delim=true) {
    unsigned char *temp = (unsigned char *)itoa(a, n);
    int i;
    for(i=0;i<n;i++) {
        LcdChr(FONT_1X, temp[i]);
    }   
    if(delim) { 
        LcdChr(FONT_1X, ' ');
    }
}

void printChar(const char c) {
    LcdChr(FONT_1X, c);
}

char buffer[255] = {0};

void init_timer() {
    // OCR2 = 255;
    // // Set to CTC Mode
    // TCCR2 |= (1 << WGM21);
    // //Set interrupt on compare match
    // TIMSK |= (1 << OCIE2);
    // // set prescaler to 64 and starts PWM
    // TCCR2 |= (1 << CS21);
    // // enable interrupts
    // sei();
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

void setup() {
    init_timer();
    init_dht();

    Lcd_init();
    LcdContrast(70);

    backlit_on();
}

int counter = 0;

void loop() {
    Lcd_clear();
    LcdGotoXY(0, 0);

    printInt(temperature, 2, false);
    printChar('C');
    printChar(' ');
    printInt(humidity, 2, false);
    printChar('%');

    Lcd_update();
    
    _delay_ms(1000);
    update_dht();
    counter ++;
}

int main(void) {
    setup();
    while(true) {
        loop();
    }
}

 
// ISR (TIMER2_COMP_vect){
//     time++;
//     if(time == 60000) {
//         time = 0;
//     }
// }
