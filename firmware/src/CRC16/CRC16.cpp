/*
    Copyright (c) 2023 Masato Kubotera

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "Arduino.h"
#include "CRC16.h"

CRC16::CRC16() {
}

uint16_t CRC16::getCrc16(uint8_t* buff, size_t length) {
    uint16_t crc = 0;
    uint16_t temp = 0;
    for (size_t count = 0; count < length; count++) {
        temp = ((buff[count] ^ (crc >> 8)) & 0xFF);
        crc = (crc_table[temp] ^ (crc << 8)) & 0xFFFF;
    }
    return crc;
}

bool CRC16::checkCrc16(uint8_t* buff, size_t length) {
    uint8_t crc_low = buff[length-2];
    uint8_t crc_high = buff[length-1];
    uint16_t crc = (uint16_t)crc_low | (uint16_t(crc_high) << 8);

    uint16_t calculated_crc = getCrc16(buff, length-2);

    if (calculated_crc != crc) {
        return false;
    }
    return true;
}
