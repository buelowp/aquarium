/*
 * Copyright (c) 2019 Peter Buelow <goballstate at gmail dot com>
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ATLASSCIENTIFICI2C_H
#define ATLASSCIENTIFICI2C_H
#include <functional>
#include <mutex>
#include <vector>

#include <onion-i2c.h>

#include "itimer.h"

#define MAX_READ_SIZE   64

class AtlasScientificI2C
{
public:
    static const int INFO = 0;
    static const int READING = 1;
    static const int STATUS = 2;

    AtlasScientificI2C(uint8_t, uint8_t);
    virtual ~AtlasScientificI2C();
    
    bool sendCommand(int, uint8_t, uint8_t*, int);
    bool sendInfoCommand();
    bool sendReadCommand();
    bool sendStatusCommand();
    
    virtual void response(int, uint8_t*, int) = 0;

    std::vector<uint8_t> m_lastResponse;
    uint8_t m_address;
    uint8_t m_device;
    int m_lastCommand;
        
private:
    void readValue();
    
    std::mutex m_commandRunning;
    uint8_t m_lastRegister;
};

#endif // ATLASSCIENTIFICI2C_H
