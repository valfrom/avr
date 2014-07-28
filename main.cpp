#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

extern "C" {
    #include "n5110/n5110.h"
    #include "dht/dht.h"
    // #include "glcd/fonts/Liberation_Sans15x21_Numbers.h"
    // #include "glcd/devices/AVR8.h"
}

volatile int64_t time=0;

dht_t dht; 
int temperature = 0;
int humidity = 0;
int speed = 0;
int avrgSpeed = 0;
int odo = 0;
int trip = 0;
int dst = 0;

int battery = 0;


volatile int64_t counter = 0;

#define COUNTER_EEPROM_ADDRESS 0

char *itoa(int64_t a, int len=0) {
    static char buf[16];
    char *p = buf + 15;
    p[0] = 0;
    int64_t v = a;
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
    DDRD |= (1 << 7);
    PORTD |= (1 << 7);
}

void backlit_off() {
    DDRD |= (1 << 7);
    PORTD &= ~(1 << 7);
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


void init_dht() {
    PORTC &= ~(1 << DHT_PIN);
    //PORTC |= (1 << DHT_PIN);
    DHT_Init(&dht, dhtproc);
}



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

void printInt(int64_t a, int n=4, LcdFontSize font=FONT_1X, int delim=true) {
    if(a < 0) {
        a = -a;
    }
    unsigned char *temp = (unsigned char *)itoa(a, n);
    int i;
    for(i=0;i<n;i++) {
        LcdChr(font, temp[i]);
    }   
    if(delim) { 
        LcdChr(font, ' ');
    }
}

void printChar(const char c, LcdFontSize font=FONT_1X) {
    LcdChr(font, c);
}

void printStr(const char *str, LcdFontSize font=FONT_1X) {
    int i;
    for(i=0;i<100;i++) {
        char c = pgm_read_byte(str + i);
        if(c == 0) {
            break;
        }
        printChar(c, font);
    }
}


void init_timers() {
    // Set the Timer Mode to CTC
    TCCR0A |= (1 << WGM01);

    // Set the value that you want to count to
    OCR0A = 0x20;

    //Set the ISR COMPA vect
    TIMSK0 |= (1 << OCIE0A);

    // set prescaler to 64 and start the timer
    TCCR0B |= (1 << CS01) | (1 << CS00);
}

void init_interrupts() {
    PIND |= (1 << DDD2);
    DDRD &= ~(1 << DDD2);

    EICRA |= (1<<ISC01) | (1<<ISC00);
    EIMSK |= (1<<INT0);
}

int oldCounter = 0;


void load_data() {
    eeprom_read_block(&oldCounter, COUNTER_EEPROM_ADDRESS, sizeof(int));
    counter = oldCounter;
}


void write_data() {
    if(oldCounter != counter) {
        oldCounter = counter;
        eeprom_write_block(&oldCounter, COUNTER_EEPROM_ADDRESS, sizeof(int));        
    }
}

void setup() {
    cli();
    load_data();
    init_interrupts();
    init_timers();
    init_dht();

    Lcd_init();
    LcdContrast(70);

    backlit_on();

    speed = 358;
    avrgSpeed = 235;
    odo = 40000;
    trip = 301;
    dst = 1234;

    sei();
}


void loop() {

    Lcd_clear();

    battery ++;
    if(battery > 100) {
        battery = 0;
    }
    int h = 18;
    int w = 10;

    Lcd_rect_empty(0, 2, w, h+2, PIXEL_ON);

    int gauge = (battery) * h / 100;
    Lcd_rect(0, h+2, gauge, w, PIXEL_ON);

    Lcd_rect(3, 1, 2, 5, PIXEL_ON);

    int i;
    for(i=0;i<4;i++) {
        int y = i * 4 + 5;
        Lcd_line(0, y, w, y, PIXEL_OFF);
    }

    
    if(speed == 0) {
        LcdGotoXY(0, 0);

        printInt(temperature, 2, FONT_1X, false);
        printStr(PSTR("C "));

        printInt(humidity, 2, FONT_1X, false);
        printChar('%');
    }

    // LcdGotoXY(8, 2);

    // printInt(avrgSpeed / 10, 2, FONT_1X, false);

    LcdGotoXY(7, 1);

    printInt(speed / 10, 2, FONT_2X, false);

    LcdGotoXY(11, 1);
    printChar('.');
    printInt(speed % 10, 1, FONT_1X, false);

    LcdGotoXY(11, 0);
    printStr(PSTR("KMH"));

    LcdGotoXY(0, 3);
    printStr(PSTR("DST "));
    printInt(dst / 10, 5, FONT_1X, false);
    printStr(PSTR("."));
    printInt(dst % 10, 1, FONT_1X, false);
    printStr(PSTR(" KM"));


    LcdGotoXY(0, 4);
    printStr(PSTR("TRP "));
    printInt(trip / 10, 5, FONT_1X, false);
    printStr(PSTR("."));
    printInt(trip % 10, 1, FONT_1X, false);
    printStr(PSTR(" KM"));

    LcdGotoXY(0, 5);
    printStr(PSTR("ODO "));
    printInt(odo / 10, 7, FONT_1X, false);
    printStr(PSTR(" KM"));


    Lcd_update();
    
    _delay_ms(100);
    update_dht();
    write_data();
}

int main(void) {
    setup();
    while(true) {
        loop();
    }
}

ISR(INT0_vect) {
    counter++;
}
 
ISR (TIMER0_COMPA_vect) {
    time++;
}
