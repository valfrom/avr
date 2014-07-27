#include "dht.h"
#include <avr/interrupt.h>

#define TIMEOUT 10000

int DHT_Init(dht_t *self, dht_service_proc_t sp){
	self->sproc = sp;
	self->temperature = 0;
	self->humidity = 0;
	return DHTLIB_OK;
}

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_TIMEOUT
static int DHT_read(dht_t *self)
{
	// INIT BUFFERVAR TO RECEIVE DATA
	uint8_t cnt = 7;
	uint8_t idx = 0;

	// EMPTY BUFFER
	int i;
	for (i=0; i< 5; i++) self->_bits[i] = 0;

	// REQUEST SAMPLE
	int sreg = SREG;
	//cli();
	
	self->sproc(DHT_WRITE_PIN, LOW);
	self->sproc(DHT_DELAY_MS, 23);
	self->sproc(DHT_WRITE_PIN, HIGH);
	self->sproc(DHT_DELAY_US, 30);

	self->sproc(DHT_READ_PIN, 0);
	self->sproc(DHT_DELAY_US, 20);

	// GET ACKNOWLEDGE or TIMEOUT
	unsigned int loopCnt = TIMEOUT;
	
	// after 50us the line should be low if everything is ok. 
	if(self->sproc(DHT_READ_PIN, 0) != LOW) {
	 	//goto timeout;
	 	return -5;
	}
		
	self->sproc(DHT_DELAY_US, 40); 
	
	// now the pin *should* be high
	if(self->sproc(DHT_READ_PIN, 0) != HIGH) {
		//return DHTLIB_ERROR_TIMEOUT; 
		return -6;
	}
		
	// now we wait until it pulls the line low again
	while(self->sproc(DHT_READ_PIN, 0) == HIGH)
		if (loopCnt-- == 0) {
			//goto timeout;
			return -7;
		}

	// READ THE OUTPUT - 40 BITS => 5 BYTES
	for (i=0; i<40; i++)
	{
		loopCnt = TIMEOUT;
		while(self->sproc(DHT_READ_PIN, 0) == LOW) {
			if (loopCnt-- == 0) {
				//goto timeout;
				return -8;
			}
		}
		
		self->sproc(DHT_DELAY_US, 40); 
		
		loopCnt = TIMEOUT;
		// if the line is high after > 30 us then it's a one
		if(self->sproc(DHT_READ_PIN, 0) == HIGH)
			self->_bits[idx] |= (1 << cnt);
		
		// wait until the next low
		while(self->sproc(DHT_READ_PIN, 0) == HIGH) {
			if (loopCnt-- == 0) {
				//goto timeout;
				return -9;
			}
		}

		if (cnt == 0){ // next byte?
			cnt = 7;  
			idx++;      
		}
		else cnt--;
	}
	SREG = sreg;
	return DHTLIB_OK;
timeout: 
	SREG = sreg;
	return DHTLIB_ERROR_TIMEOUT;
}

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int DHT_Read11(dht_t *self)
{
        // READ VALUES
        int rv = DHT_read(self);
        if (rv != DHTLIB_OK)
    {
                self->humidity    = DHTLIB_INVALID_VALUE; // or is NaN prefered?
                self->temperature = DHTLIB_INVALID_VALUE;
                return rv;
        }

        // CONVERT AND STORE
        self->humidity    = self->_bits[0];  // bit[1] == 0;
        self->temperature = self->_bits[2];  // bits[3] == 0;

        // TEST CHECKSUM
        uint8_t sum = self->_bits[0] + self->_bits[2]; // bits[1] && bits[3] both 0
        if (self->_bits[4] != sum) return DHTLIB_ERROR_CHECKSUM;

        return DHTLIB_OK;
}

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int DHT_Read22(dht_t *self)
{
        // READ VALUES
        int rv = DHT_read(self);
        if (rv != DHTLIB_OK)
    {
                self->humidity    = DHTLIB_INVALID_VALUE;  // invalid value, or is NaN prefered?
                self->temperature = DHTLIB_INVALID_VALUE;  // invalid value
                return rv;
        }

        // CONVERT AND STORE
        self->humidity    = WORD(self->_bits[0], self->_bits[1]) * 0.1;

        if (self->_bits[2] & 0x80) // negative temperature
        {
                        self->temperature = WORD(self->_bits[2]&0x7F, self->_bits[3]) * 0.1;
                        self->temperature *= -1.0;
        }
        else
        {
                        self->temperature = WORD(self->_bits[2], self->_bits[3]) * 0.1;
        }

        // TEST CHECKSUM
        uint8_t sum = self->_bits[0] + self->_bits[1] + self->_bits[2] + self->_bits[3];
        if (self->_bits[4] != sum) return DHTLIB_ERROR_CHECKSUM;

        return DHTLIB_OK;
}

