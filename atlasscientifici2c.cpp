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
    m_address(address), m_device(device)
{
    char filename[40];
    sprintf(filename, "/dev/i2c-%d", m_device);
        
    m_enabled = true;

    if ((m_fd = open(filename, O_RDWR)) < 0) {
        syslog(LOG_ERR, "Failed to open i2c device %d", m_device);
        m_enabled = false;
    }

    if (ioctl(m_fd, I2C_SLAVE, m_address) < 0) {
        syslog(LOG_ERR, "Failed to acquire bus access and/or talk to slave at address %x", m_address);
        m_enabled = false;
    }
}

AtlasScientificI2C::~AtlasScientificI2C()
{
    close(m_fd);
    m_enabled = false;
}

bool AtlasScientificI2C::sendCommand(int cmd, uint8_t *buf, int size, int delay)
{
    m_commandRunning.lock();
    
    m_lastCommand = cmd;
    
    if (write(m_fd, buf, size) > 0) {
        t.setTimeout(std::bind(&AtlasScientificI2C::readValue, this), delay);
        return true;
    }
    else {
        syslog(LOG_ERR, "Error writing i2c event\n");
    }
    return false;
}

void AtlasScientificI2C::readValue()
{
    uint8_t buffer[MAX_READ_SIZE];
    int index = 0;
    int bytes = 0;
    
    memset(buffer, 0, MAX_READ_SIZE);
    
    if ((bytes = read(m_fd, buffer, MAX_READ_SIZE)) > 0) {
        for (int i = 0; i < MAX_READ_SIZE; i++) {
            if (buffer[i] == 0) {
                index = i;
                break;
            }
            m_lastResponse.push_back(buffer[i]);
        }
        response(m_lastCommand, buffer, index);
    }
    else {
        std::cerr << "Unable to read value from i2c device" << std::endl;
        syslog(LOG_ERR, "Unable to read from i2c device at address %x", m_address);
    }
    m_commandRunning.unlock();
}

bool AtlasScientificI2C::sendInfoCommand()
{
    uint8_t i[1] = {'i'};
    return sendCommand(INFO, i, 1, 300);
}

bool AtlasScientificI2C::sendStatusCommand()
{
    uint8_t i[] = {'s','t','a','t','u','s'};
    return sendCommand(STATUS, i, 6, 300);
}

bool AtlasScientificI2C::sendReadCommand()
{
    uint8_t i[1] = {'r'};
    return sendCommand(READING, i, 1, 900);
}

std::vector<std::string> AtlasScientificI2C::split(const std::string& s, char delimiter)
{
   std::vector<std::string> tokens;
   std::string token;
   std::istringstream tokenStream(s);
   while (std::getline(tokenStream, token, delimiter))
   {
        token.erase(std::remove_if(token.begin(), token.end(), []( auto const& c ) -> bool { return !std::isprint(c); } ), token.end());
        tokens.push_back(token);
   }
   return tokens;
}
