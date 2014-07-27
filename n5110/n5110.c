/*
 * Описание     :  Это драйвер для графического LCD от Nokia 5110, а также его китайских клонов.
 * Автор        :  Xander Gresolio <xugres@gmail.com>
 * Веб-страница :  https://github.com/gresolio/N3310Lib
 * Лицензия     :  GPL v3.0
 *
 * Оптимизация  : kobzar aka kobraz для http://cxem.net/ maodzedun@gmail.com
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "n5110.h"

//Работа с float
int gpow(int n, int power) {
	int res = 1;
	while(power--) res *= n;
	return res;
}

/*
 * Имя                   :  gftoa
 * Описание              :  Переводит float в string
 * Аргумент(ы)           :  n - число, power -  число знаков после запятой
 * Возвращаемое значение :  string
 */

char *gftoa(float f, int dec) {
	static char buf[16];
	char *p = buf + 15;
	int i = f * gpow(10, dec);
	int sign = i < 0 ? -1 : 1;
	i *= sign;
	do {
		*--p = '0' + (i % 10);
		i /= 10;
		if (--dec == 0) *--p = '.';
	}
	while (i != 0);

	if (dec > 0) {
		while (dec-- > 0)
		*--p = '0';
		*--p = '.';
	}
	
	if (*p == '.') *--p = '0';

	if (sign < 0) *--p = '-';

	return p;
}

// Прототипы приватных функций драйвера

static void LcdSend    ( byte data, LcdCmdData cd );
static void Delay      ( void );

// Глобальные переменные

// Кэш в ОЗУ 84*48 бит или 504 байта
static byte  LcdCache [ LCD_CACHE_SIZE ];

// Чтобы не обновлять весь дисплей, а лишь ту часть что изменилась,
// будем отмечать две границы кэша где произошли изменения. Затем
// можно копировать эту часть кэша между границами в ОЗУ дисплея.
static int   LoWaterMark;   // нижняя граница
static int   HiWaterMark;   // верхняя граница

// Указатель для работы с LcdCache[]
static int   LcdCacheIdx;

// Флаг изменений кэша
static byte  UpdateLcd;


/*
 * Имя                   :  Lcd_init
 * Описание              :  Производит инициализацию порта и SPI МК, контроллера LCD
 * Аргумент(ы)           :  Нет
 * Возвращаемое значение :  Нет
 */
void Lcd_init ( void )
{
    // Pull-up на вывод подключенный к reset дисплея
    LCD_PORT |= _BV ( LCD_RST_PIN );

    // Устанавливаем нужные биты порта на выход
    LCD_DDR |= _BV( LCD_RST_PIN ) | _BV( LCD_DC_PIN ) | _BV( LCD_CE_PIN ) | _BV( SPI_MOSI_PIN ) | _BV( SPI_CLK_PIN );

    // Некалиброванная задержка
    Delay();

    // Дергаем reset
    LCD_PORT &= ~( _BV( LCD_RST_PIN ) );
    Delay();
    LCD_PORT |= _BV ( LCD_RST_PIN );

    // Активируем SPI:
    // без прерываний, старший бит первый, режим мастера, CPOL->0, CPHA->0, Clk/4
    SPCR = 0x50;

    // Отключаем LCD контроллер - высокий уровень на SCE
    LCD_PORT |= _BV( LCD_CE_PIN );

    // Отправляем команды дисплею
    LcdSend( 0x21, LCD_CMD ); // Включаем расширенный набор команд (LCD Extended Commands)
    LcdSend( 0xC8, LCD_CMD ); // Установка контрастности (LCD Vop)
    LcdSend( 0x06, LCD_CMD ); // Установка температурного коэффициента (Temp coefficent)
    LcdSend( 0x13, LCD_CMD ); // Настройка питания (LCD bias mode 1:48)
    LcdSend( 0x20, LCD_CMD ); // Включаем стандартный набор команд и горизонтальную адресацию (LCD Standard Commands,Horizontal addressing mode)
    LcdSend( 0x0C, LCD_CMD ); // Нормальный режим (LCD in normal mode)

    // Первичная очистка дисплея
    Lcd_clear();
    Lcd_update();
}


/*
 * Имя                   :  Lcd_clear
 * Описание              :  Очищает дисплей. Далее необходимо выполнить LcdUpdate
 * Аргумент(ы)           :  Нет
 * Возвращаемое значение :  Нет
 */
void Lcd_clear ( void )
{

    memset( LcdCache, 0x00, LCD_CACHE_SIZE );
    
    // Сброс указателей границ в максимальное значение
    LoWaterMark = 0;
    HiWaterMark = LCD_CACHE_SIZE - 1;

    // Установка флага изменений кэша
    UpdateLcd = TRUE;
}



/*
 * Имя                   :  Lcd_update
 * Описание              :  Копирует кэш в ОЗУ дисплея
 * Аргумент(ы)           :  Нет
 * Возвращаемое значение :  Нет
 */
void Lcd_update (void)
{
    int i;

    if ( LoWaterMark < 0 )
        LoWaterMark = 0;
    else if ( LoWaterMark >= LCD_CACHE_SIZE )
        LoWaterMark = LCD_CACHE_SIZE - 1;

    if ( HiWaterMark < 0 )
        HiWaterMark = 0;
    else if ( HiWaterMark >= LCD_CACHE_SIZE )
        HiWaterMark = LCD_CACHE_SIZE - 1;

    #ifdef CHINA_LCD  // Алгоритм для китайского ЖК из нестандартным контроллером

        byte x,y;

        // 102 x 64 - таково предполагаемое разрешение буфера китайского ЖК, при чем
        // память буфера отображается на дисплей со сдвигом вверх на 3 пикселя.
        // Поэтому выводим картинку ниже - начиная с второй строки y+1, а потом
        // сдвинем вверх (опять таки фича китайца, полезная в данном случае)
                
        x = LoWaterMark % LCD_X_RES;      // Устанавливаем начальный адрес x
        LcdSend( 0x80 | x, LCD_CMD );     // относительно нижней границы LoWaterMark
        
        y = LoWaterMark / LCD_X_RES + 1;  // Устанавливаем начальный адрес y+1
        LcdSend( 0x40 | y, LCD_CMD );     // относительно нижней границы LoWaterMark

        for ( i = LoWaterMark; i <= HiWaterMark; i++ )
        {
            // передаем данные в буфер дисплея
            LcdSend( LcdCache[i], LCD_DATA );
            
            x++;                 // заодно подсчитываем координату x, чтобы вовремя перейти на новую строку
            if (x >= LCD_X_RES)  // если вышли за предел, то переходим на следующую строку (x=0; y++)
            {
                // проще говоря, чтобы верно заполнить нужную часть нестандартного буфера,
                // придется явно указывать требуемый адрес, иначе все поплывет :)
                x=0;                
                LcdSend( 0x80, LCD_CMD );
                y++;
                LcdSend( 0x40 | y, LCD_CMD );
            }
        }

        LcdSend( 0x21, LCD_CMD );    // Включаем расширенный набор команд
        LcdSend( 0x45, LCD_CMD );    // Сдвигаем картинку на 5 пикселей вверх (нестандартная команда китайца, оригинал её игнорирует)
        LcdSend( 0x20, LCD_CMD );    // Включаем стандартный набор команд и горизонтальную адресацию

    #else  // Алгоритм для оригинального дисплея

        // Устанавливаем начальный адрес в соответствии к LoWaterMark
        LcdSend( 0x80 | ( LoWaterMark % LCD_X_RES ), LCD_CMD );
        LcdSend( 0x40 | ( LoWaterMark / LCD_X_RES ), LCD_CMD );

        // Обновляем необходимую часть буфера дисплея
        for ( i = LoWaterMark; i <= HiWaterMark; i++ )
        {
            // Для оригинального дисплея не нужно следить за адресом в буфере,
            // можно просто последовательно выводить данные
            LcdSend( LcdCache[i], LCD_DATA );
        }

    #endif

    // Сброс указателей границ в пустоту
    LoWaterMark = LCD_CACHE_SIZE - 1;
    HiWaterMark = 0;

    // Сброс флага изменений кэша
    UpdateLcd = FALSE;
}


/*
 * Имя                   :  LcdSend
 * Описание              :  Отправляет данные в контроллер дисплея
 * Аргумент(ы)           :  data -> данные для отправки
 *                          cd   -> команда или данные (смотри enum в n5110.h)
 * Возвращаемое значение :  Нет
 */
static void LcdSend ( byte data, LcdCmdData cd )
{
    // Включаем контроллер дисплея (низкий уровень активный)
    LCD_PORT &= ~( _BV( LCD_CE_PIN ) );

    if ( cd == LCD_DATA )
    {
        LCD_PORT |= _BV( LCD_DC_PIN );
    }
    else
    {
        LCD_PORT &= ~( _BV( LCD_DC_PIN ) );
    }

    // Отправка данных в контроллер дисплея
    SPDR = data;

    // Ждем окончания передачи
    while ( (SPSR & 0x80) != 0x80 );

    // Отключаем контроллер дисплея
    LCD_PORT |= _BV( LCD_CE_PIN );
}


/*
 * Имя                   :  LcdContrast
 * Описание              :  Устанавливает контрастность дисплея
 * Аргумент(ы)           :  контраст -> значение от 0x00 к 0x7F
 * Возвращаемое значение :  Нет
 */
void LcdContrast ( byte contrast )
{
    LcdSend( 0x21, LCD_CMD );              // Расширенный набор команд
    LcdSend( 0x80 | contrast, LCD_CMD );   // Установка уровня контрастности
    LcdSend( 0x20, LCD_CMD );              // Стандартный набор команд, горизонтальная адресация
}


/*
 * Имя                   :  Delay
 * Описание              :  Некалиброванная задержка для процедуры инициализации LCD
 * Аргумент(ы)           :  Нет
 * Возвращаемое значение :  Нет
 */
static void Delay ( void )
{
    int i;

    for ( i = -32000; i < 32000; i++ );
}


/*
 * Имя                   :  LcdGotoXY
 * Описание              :  Устанавливает курсор в позицию x,y относительно стандартного размера шрифта
 * Аргумент(ы)           :  x,y -> координаты новой позиции курсора. Значения: 0,0 .. 13,5
 * Возвращаемое значение :  смотри возвращаемое значение в n5110.h
 */
byte LcdGotoXY ( byte x, byte y )
{
    // Проверка границ
    if( x > 13 || y > 5 ) return OUT_OF_BORDER;

    //  Вычисление указателя. Определен как адрес в пределах 504 байт
    LcdCacheIdx = x * 6 + y * 84;
    return OK;
}


/*
 * Имя                   :  LcdChr
 * Описание              :  Выводит символ в текущей позиции курсора, затем инкрементирует положение курсора
 * Аргумент(ы)           :  size -> размер шрифта. Смотри enum в n5110.h
 *                          ch   -> символ для вывода
 * Возвращаемое значение :  смотри возвращаемое значение в n5110lcd.h
 */
byte LcdChr ( LcdFontSize size, byte ch )
{
    byte i, c;
    byte b1, b2;
    int  tmpIdx;

    if ( LcdCacheIdx < LoWaterMark )
    {
        // Обновляем нижнюю границу
        LoWaterMark = LcdCacheIdx;
    }

    if ( (ch >= 0x20) && (ch <= 0x7F) )
    {
        // Смещение в таблице для символов ASCII[0x20-0x7F]
        ch -= 32;
    }
    else if ( ch >= 0xC0 )
    {
        // Смещение в таблице для символов CP1251[0xC0-0xFF]
        ch -= 96;
    }
    else
    {
        // Остальные игнорируем (их просто нет в таблице для экономии памяти)
        ch = 95;
    }

    if ( size == FONT_1X )
    {
        for ( i = 0; i < 5; i++ )
        {
            // Копируем вид символа из таблицы в кэш
            LcdCache[LcdCacheIdx++] = pgm_read_byte( &(FontLookup[ch][i]) ) << 1;
        }
    }
    else if ( size == FONT_2X )
    {
        tmpIdx = LcdCacheIdx - 84;

        if ( tmpIdx < LoWaterMark )
        {
            LoWaterMark = tmpIdx;
        }

        if ( tmpIdx < 0 ) return OUT_OF_BORDER;

        for ( i = 0; i < 5; i++ )
        {
            // Копируем вид символа из таблицы у временную переменную
            c = pgm_read_byte(&(FontLookup[ch][i])) << 1;
            // Увеличиваем картинку
            // Первую часть
            b1 =  (c & 0x01) * 3;
            b1 |= (c & 0x02) * 6;
            b1 |= (c & 0x04) * 12;
            b1 |= (c & 0x08) * 24;

            c >>= 4;
            // Вторую часть
            b2 =  (c & 0x01) * 3;
            b2 |= (c & 0x02) * 6;
            b2 |= (c & 0x04) * 12;
            b2 |= (c & 0x08) * 24;

            // Копируем две части в кэш
            LcdCache[tmpIdx++] = b1;
            LcdCache[tmpIdx++] = b1;
            LcdCache[tmpIdx + 82] = b2;
            LcdCache[tmpIdx + 83] = b2;
        }

        // Обновляем x координату курсора
        LcdCacheIdx = (LcdCacheIdx + 11) % LCD_CACHE_SIZE;
    }

    if ( LcdCacheIdx > HiWaterMark )
    {
        // Обновляем верхнюю границу
        HiWaterMark = LcdCacheIdx;
    }

    // Горизонтальный разрыв между символами
    LcdCache[LcdCacheIdx] = 0x00;
    // Если достигли позицию указателя LCD_CACHE_SIZE - 1, переходим в начало
    if(LcdCacheIdx == (LCD_CACHE_SIZE - 1) )
    {
        LcdCacheIdx = 0;
        return OK_WITH_WRAP;
    }
    // Иначе просто инкрементируем указатель
    LcdCacheIdx++;
    return OK;
}


/*
 * Имя                   :  Lcd_print
 * Описание              :  Эта функция предназначена для печати строки из переменной
 * Аргумент(ы)           :  size      -> размер шрифта. Смотри enum в n5110.h
 *                       :  dataArray -> массив содержащий строку которую нужно напечатать
 *						 :  x,y -> координаты
 * Возвращаемое значение :  смотри возвращаемое значение в n5110lcd.h
 * Пример                :  LcdFStr(0, 0, FONT_1X,(unsigned char*)some_char);
 *                          LcdFStr(0, 0, FONT_1X, &name_of_string_as_array);
 */
byte Lcd_print ( byte x, byte y, LcdFontSize size, byte dataArray[] )
{
	LcdGotoXY(x,y);
    byte tmpIdx=0;
    byte response;
    while( dataArray[ tmpIdx ] != '\0' )
    {
        // Выводим символ
        response = LcdChr( size, dataArray[ tmpIdx ] );
        // Не стоит волноваться если произойдет OUT_OF_BORDER,
        // строка будет печататься дальше из начала дисплея
        if( response == OUT_OF_BORDER)
            return OUT_OF_BORDER;
        // Увеличиваем указатель
        tmpIdx++;
    }
    return OK;
}



/*
 * Имя                   :  Lcd_prints
 * Описание              :  Эта функция предназначена для печати статичной строки 
 * Аргумент(ы)           :  size    -> размер шрифта. Смотри enum в n5110.h
 *                          dataPtr -> указатель на строку которую нужно напечатать
 *						 :  x,y -> координаты
 * Возвращаемое значение :  смотри возвращаемое значение в n5110lcd.h
 * Пример                :  LcdFStr(0, 0, FONT_1X, PSTR("Hello World"));
 *                          LcdFStr(0, 0, FONT_1X, &name_of_string_as_array);
 */
byte Lcd_prints ( byte x, byte y, LcdFontSize size, const byte *dataPtr )
{
    LcdGotoXY(x,y);
	byte c;
    byte response;
    for ( c = pgm_read_byte( dataPtr ); c; ++dataPtr, c = pgm_read_byte( dataPtr ) )
    {
        // Выводим символ
        response = LcdChr( size, c );
        if(response == OUT_OF_BORDER)
            return OUT_OF_BORDER;
    }

    return OK;
}


/*
 * Имя                   :  Lcd_printf
 * Описание              :  Эта функция предназначена для печати числа с плавающей запятой
 * Аргумент(ы)           :  size      -> размер шрифта. Смотри enum в n5110.h
 *                       :  data -> число
 *						 :	accuracy -> число знаков после запятой
 *						 :  x,y -> координаты
 * Возвращаемое значение :  смотри возвращаемое значение в n5110lcd.h
 * Пример                :  LcdFStr(0, 0, FONT_1X, float_var , 2);
 *                          LcdFStr(0, 0, FONT_1X, &name_of_string_as_array);
 */
void Lcd_printf ( byte x, byte y, LcdFontSize size, float data, int accuracy )
{
	Lcd_print(x, y, size, (unsigned char*)gftoa(data, accuracy));
}



/*
 * Имя                   :  Lcd_pixel
 * Описание              :  Отображает пиксель по абсолютным координатам (x,y)
 * Аргумент(ы)           :  x,y  -> абсолютные координаты пикселя
 *                          mode -> Off, On или Xor. Смотри enum в n5110.h
 * Возвращаемое значение :  смотри возвращаемое значение в n5110lcd.h
 */
byte Lcd_pixel ( byte x, byte y, LcdPixelMode mode )
{
    int  index;
    byte  offset;
    byte  data;

    // Защита от выхода за пределы
    if ( x >= LCD_X_RES || y >= LCD_Y_RES) return OUT_OF_BORDER;

    // Пересчет индекса и смещения
    index = ( ( y / 8 ) * 84 ) + x;
    offset  = y - ( ( y / 8 ) * 8 );

    data = LcdCache[ index ];

    // Обработка битов

    // Режим PIXEL_OFF
    if ( mode == PIXEL_OFF )
    {
        data &= ( ~( 0x01 << offset ) );
    }
    // Режим PIXEL_ON
    else if ( mode == PIXEL_ON )
    {
        data |= ( 0x01 << offset );
    }
    // Режим PIXEL_XOR
    else if ( mode  == PIXEL_XOR )
    {
        data ^= ( 0x01 << offset );
    }

    // Окончательный результат копируем в кэш
    LcdCache[ index ] = data;

    if ( index < LoWaterMark )
    {
        // Обновляем нижнюю границу
        LoWaterMark = index;
    }

    if ( index > HiWaterMark )
    {
        // Обновляем верхнюю границу
        HiWaterMark = index;
    }
    return OK;
}



/*
 * Имя                   :  Lcd_line
 * Описание              :  Рисует линию между двумя точками на дисплее (алгоритм Брезенхэма)
 * Аргумент(ы)           :  x1, y1  -> абсолютные координаты начала линии
 *                          x2, y2  -> абсолютные координаты конца линии
 *                          mode    -> Off, On или Xor. Смотри enum в n5110.h
 * Возвращаемое значение :  смотри возвращаемое значение в n5110lcd.h
 */
byte Lcd_line ( byte x1, byte y1, byte x2, byte y2, LcdPixelMode mode )
{
    int dx, dy, stepx, stepy, fraction;
    byte response;

    // dy   y2 - y1
    // -- = -------
    // dx   x2 - x1

    dy = y2 - y1;
    dx = x2 - x1;

    // dy отрицательное
    if ( dy < 0 )
    {
        dy    = -dy;
        stepy = -1;
    }
    else
    {
        stepy = 1;
    }

    // dx отрицательное
    if ( dx < 0 )
    {
        dx    = -dx;
        stepx = -1;
    }
    else
    {
        stepx = 1;
    }

    dx <<= 1;
    dy <<= 1;

    // Рисуем начальную точку
    response = Lcd_pixel( x1, y1, mode );
    if(response)
        return response;

    // Рисуем следующие точки до конца
    if ( dx > dy )
    {
        fraction = dy - ( dx >> 1);
        while ( x1 != x2 )
        {
            if ( fraction >= 0 )
            {
                y1 += stepy;
                fraction -= dx;
            }
            x1 += stepx;
            fraction += dy;

            response = Lcd_pixel( x1, y1, mode );
            if(response)
                return response;

        }
    }
    else
    {
        fraction = dx - ( dy >> 1);
        while ( y1 != y2 )
        {
            if ( fraction >= 0 )
            {
                x1 += stepx;
                fraction -= dy;
            }
            y1 += stepy;
            fraction += dx;

            response = Lcd_pixel( x1, y1, mode );
            if(response)
                return response;
        }
    }

    // Установка флага изменений кэша
    UpdateLcd = TRUE;
    return OK;
}



/*
 * Имя                   :  Lcd_circle
 * Описание              :  Рисует окружность (алгоритм Брезенхэма)
 * Аргумент(ы)           :  x, y   -> абсолютные координаты центра
 *                          radius -> радиус окружности
 *                          mode   -> Off, On или Xor. Смотри enum в n5110.h
 * Возвращаемое значение :  смотри возвращаемое значение в n5110lcd.h
 */
byte Lcd_circle(byte x, byte y, byte radius, LcdPixelMode mode)
{
    signed char xc = 0;
    signed char yc = 0;
    signed char p = 0;

    if ( x >= LCD_X_RES || y >= LCD_Y_RES) return OUT_OF_BORDER;

    yc = radius;
    p = 3 - (radius<<1);
    while (xc <= yc)  
    {
        Lcd_pixel(x + xc, y + yc, mode);
        Lcd_pixel(x + xc, y - yc, mode);
        Lcd_pixel(x - xc, y + yc, mode);
        Lcd_pixel(x - xc, y - yc, mode);
        Lcd_pixel(x + yc, y + xc, mode);
        Lcd_pixel(x + yc, y - xc, mode);
        Lcd_pixel(x - yc, y + xc, mode);
        Lcd_pixel(x - yc, y - xc, mode);
        if (p < 0) p += (xc++ << 2) + 6;
            else p += ((xc++ - yc--)<<2) + 10;
    }

    // Установка флага изменений кэша
    UpdateLcd = TRUE;
    return OK;
}


/*
 * Имя                   :  Lcd_rect  (rectangle)
 * Описание              :  Рисует один закрашенный прямоугольник
 * Аргумент(ы)           :  baseX  -> абсолютная координата x (нижний левый угол)
 *                          baseY  -> абсолютная координата y (нижний левый угол)
 *                          height -> высота (в пикселях)
 *                          width  -> ширина (в пикселях)
 *                          mode   -> Off, On или Xor. Смотри enum в n5110.h
 * Возвращаемое значение :  смотри возвращаемое значение в n5110lcd.h
 */
byte Lcd_rect ( byte baseX, byte baseY, byte height, byte width, LcdPixelMode mode )
{
    byte tmpIdxX,tmpIdxY,tmp;

    byte response;

    // Проверка границ
    if ( ( baseX >= LCD_X_RES) || ( baseY >= LCD_Y_RES) ) return OUT_OF_BORDER;

    if ( height > baseY )
        tmp = 0;
    else
        tmp = baseY - height + 1;

    // Рисование линий
    for ( tmpIdxY = tmp; tmpIdxY <= baseY; tmpIdxY++ )
    {
        for ( tmpIdxX = baseX; tmpIdxX < (baseX + width); tmpIdxX++ )
        {
            response = Lcd_pixel( tmpIdxX, tmpIdxY, mode );
            if(response)
                return response;

        }
    }

    // Установка флага изменений кэша
    UpdateLcd = TRUE;
    return OK;
}






/*
 * Имя                   :  Lcd_rect_empty
 * Описание              :  Рисует незакрашенный прямоугольник
 * Аргумент(ы)           :  x1    -> абсолютная координата x левого верхнего угла
 *                          y1    -> абсолютная координата y левого верхнего угла
 *                          x2    -> абсолютная координата x правого нижнего угла
 *                          y2    -> абсолютная координата y правого нижнего угла
 *                          mode  -> Off, On или Xor. Смотри enum в n5110.h
 * Возвращаемое значение :  смотри возвращаемое значение в n5110lcd.h
 */
byte Lcd_rect_empty ( byte x1, byte y1, byte x2, byte y2, LcdPixelMode mode )
{
    byte tmpIdx;

    // Проверка границ
    if ( ( x1 >= LCD_X_RES) ||  ( x2 >= LCD_X_RES) || ( y1 >= LCD_Y_RES) || ( y2 >= LCD_Y_RES) )
        return OUT_OF_BORDER;

    if ( ( x2 > x1 ) && ( y2 > y1 ) )
    {
        // Рисуем горизонтальные линии
        for ( tmpIdx = x1; tmpIdx <= x2; tmpIdx++ )
        {
            Lcd_pixel( tmpIdx, y1, mode );
            Lcd_pixel( tmpIdx, y2, mode );
        }

        // Рисуем вертикальные линии
        for ( tmpIdx = y1; tmpIdx <= y2; tmpIdx++ )
        {
            Lcd_pixel( x1, tmpIdx, mode );
            Lcd_pixel( x2, tmpIdx, mode );
        }

        // Установка флага изменений кэша
        UpdateLcd = TRUE;
    }
    return OK;
}

