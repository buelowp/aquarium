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

#include "atlasscientifici2c.h"

AtlasScientificI2C::AtlasScientificI2C(uint8_t device, uint8_t address) : 
    m_device(device), m_address(address)
{
}

AtlasScientificI2C::~AtlasScientificI2C()
{
}

bool AtlasScientificI2C::sendCommand(int cmd, uint8_t reg, uint8_t *buf, int size)
{
    int timeout = 300;
    ITimer t;
    
    std::lock_guard<std::mutex> lock(m_commandRunning);
    
    if (buf[0] == 'R' || buf[0] == 'r')
        timeout = 600;
    
    m_lastCommand = cmd;
    
    if (i2c_writeBuffer(m_device, m_address, reg, buf, size) == EXIT_SUCCESS) {
        t.setTimeout([this]() { readValue(); }, timeout);
        m_lastRegister = reg;
        return true;
    }
    return false;
}

void AtlasScientificI2C::readValue()
{
    uint8_t buffer[MAX_READ_SIZE];
    int index = 0;
    
    std::lock_guard<std::mutex> lock(m_commandRunning);
    
    memset(buffer, 0, MAX_READ_SIZE);
    
    if (i2c_read(m_device, m_address, m_lastRegister, buffer, MAX_READ_SIZE) == EXIT_SUCCESS) {
        for (int i = 0; i < MAX_READ_SIZE; i++) {
            if (buffer[i] == 0) {
                index = i;
                break;
            }
            m_lastResponse.push_back(buffer[i]);
        }
        response(m_lastCommand, buffer, index);
    }
}

bool AtlasScientificI2C::sendInfoCommand()
{
    uint8_t i[1] = {'i'};
    return sendCommand(INFO, 0, i, 1);
}

bool AtlasScientificI2C::sendStatusCommand()
{
    uint8_t i[] = {'s','t','a','t','u','s'};
    return sendCommand(STATUS, 0, i, 6);
}

bool AtlasScientificI2C::sendReadCommand()
{
    uint8_t i[1] = {'r'};
    return sendCommand(READING, 0, i, 1);
}
