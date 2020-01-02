/*
 * Copyright (c) 2020 Peter Buelow <email>
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

#ifndef MCP3008_H
#define MCP3008_H

#include <iostream>
#include <string>
#include <cstdint>
#include <cstring>
#include <errno.h>
#include <unistd.h>
#include <cstdlib>
#include <syslog.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#define ARRAY_SIZE(array) sizeof(array) / sizeof(array[0])

class MCP3008
{
public:
    static constexpr uint8_t MODE = SPI_MODE_0;
    static constexpr uint8_t BITS = 8;
    static constexpr uint32_t CLOCK = 1000000;
    static constexpr uint16_t DELAY = 5;
    
    MCP3008(std::string, int);
    ~MCP3008();

    int reading(int);
    
private:
    uint8_t control_bits(uint8_t channel);
    uint8_t control_bits_differential(uint8_t channel);
    
    int m_channels;
    int m_fd;
    bool m_enabled;
};

#endif // MCP3008_H
