
/*
 Arduino code to read data from an Elster A100C electricity meter.

 Copyright (C) 2012 Dave Berkeley solar@rotwang.co.uk

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 USA
*/

// set pin numbers:
const int ledPin = 13;
const int intPin = 2;

#define BIT_PERIOD 416 // us
#define BIT_MARGIN 10 // us

  /*
   *  Buffer
   */

#define BUFF_SIZE 128

#include "elster.h"


  /*
   *  Called on every InfraRed pulse, ie. every '1' bit.
   */

typedef unsigned long stamp;

static stamp last_us;

#define BITS(t) (((t) + (BIT_PERIOD/2)) / BIT_PERIOD)

static struct Buffer bits;

void on_change(void)
{
  //  push the number of bit periods since the last interrupt to a buffer.
  const stamp us = micros();
  const int diff = us - last_us;
  last_us = us;

  const int bit_periods = BITS(diff);
  buff_add(& bits, bit_periods);
}

  /*
   *  Decode bit stream
   */

static struct Buffer bytes;

static int bit_data;
static int bit_index;

int add_bit(int state)
{
    bit_data >>= 1;

    if (state)
        bit_data += 0x200;

    if (bit_data)
        bit_index += 1;

    if (bit_index < 10)
        return 0;

    if (bit_data & 1) // start bit
    {
        if (!state) // stop bit
        {
            const int d = (bit_data >> 1) & 0xFF;
            // high represents '0', so invert the bits
            buff_add(& bytes, (d ^ 0xFF));
            bit_data = bit_index = 0;
            return 1;
        }
    }

    //  bad frame
    bit_data = bit_index = 0;
    return 0;
}

static void on_timeout()
{
  // too long since last bits count
  
  if (bit_index)
    while (bit_data) // flush with trailing '0' bits
      add_bit(0);  
  bit_data = bit_index = 0;
}

static void on_bits(int bits)
{
  // called on each bits count.
  // the bit periods represent a '1' followed by N '0's.

  if (bits < 1)
  {
    on_timeout();
    return;
  }

  // the elapsed bit periods since last interrupt
  // represents a '1' followed by 0 or more '0's.  
  add_bit(1);
  for (; bits > 1; bits--)
    add_bit(0);
}

  /*
   *
   */
   
static int decode_bit_stream(void)
{
  // look for a pause in the bit stream
  const stamp us = micros();
  const int diff = us - last_us;
  if (BITS(diff) > 20)
    on_timeout();

  // read the bit stream  
  int bit_count;
  if (buff_get(& bits, & bit_count) < 0)
    return -1;

  on_bits(bit_count);

  // read the byte stream decoded above
  int byte_data;
  if (buff_get(& bytes, & byte_data) < 0)
    return -1;

  return byte_data;
}

typedef void (*on_reading)(unsigned long reading);


  /*
   *
   */

void meter_reading(unsigned long reading)
{
  Serial.print(reading);
  Serial.print("\r\n");

  static int state = 0;
  digitalWrite(ledPin, state = !state);
}

ElsterA100C meter(meter_reading);

void setup() 
{
  buff_init(& bits);
  buff_init(& bytes);

  pinMode(ledPin, OUTPUT);
  pinMode(intPin, INPUT);

  attachInterrupt(0, on_change, RISING);

  Serial.begin(9600);

  Serial.println("ready");
}

void loop()
{
  int byte_data = decode_bit_stream();
  if (byte_data == -1)
    return;

  meter.on_data(byte_data);
}

// FIN
