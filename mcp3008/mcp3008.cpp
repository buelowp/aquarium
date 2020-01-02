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

#include "mcp3008.h"

MCP3008::MCP3008(std::string device, int channels) : m_channels(channels), m_enabled(true)
{
    if ((m_fd = open(device.c_str(), O_RDWR)) < 0) {
        syslog(LOG_ERR, "open: %s %s(%d) ", device.c_str(), strerror(errno), errno);
        std::cerr << __FUNCTION__ << ": open: " << device << " " << strerror(errno) << errno << std::endl;
        m_enabled = false;
    }
    
    if (ioctl(m_fd, SPI_IOC_WR_MODE, &MCP3008::MODE) == -1) {
        syslog(LOG_ERR, "ioctl: SPI_IOC_WR_MODE %s(%d)", strerror(errno), errno);
        std::cerr << __FUNCTION__ << ": ioctl: SPI_IOC_WR_MODE " << strerror(errno) << errno << std::endl;
        m_enabled = false;
    }

    if (ioctl(m_fd, SPI_IOC_WR_BITS_PER_WORD, &MCP3008::BITS) == -1) {
        syslog(LOG_ERR, "ioctl: SPI_IOC_WR_BITS_PER_WORD %s(%d)", strerror(errno), errno);
        std::cerr << __FUNCTION__ << ": ioctl: SPI_IOC_WR_BITS_PER_WORD " << strerror(errno) << errno << std::endl;
        m_enabled = false;
    }

    if (ioctl(m_fd, SPI_IOC_WR_MAX_SPEED_HZ, &MCP3008::CLOCK) == -1) {
        syslog(LOG_ERR, "ioctl: SPI_IOC_WR_MAX_SPEED_HZ %s(%d)", strerror(errno), errno);
        std::cerr << __FUNCTION__ << ": ioctl: SPI_IOC_WR_MAX_SPEED_HZ " << strerror(errno) << errno << std::endl;
        m_enabled = false;
    }

    if (ioctl(m_fd, SPI_IOC_RD_MAX_SPEED_HZ, &MCP3008::CLOCK) == -1) {
        syslog(LOG_ERR, "ioctl: SPI_IOC_RD_MAX_SPEED_HZ %s(%d)", strerror(errno), errno);
        std::cerr << __FUNCTION__ << ": ioctl: SPI_IOC_RD_MAX_SPEED_HZ " << strerror(errno) << errno << std::endl;
        m_enabled = false;
    }
}

MCP3008::~MCP3008()
{
    close(m_fd);
}

uint8_t MCP3008::control_bits_differential(uint8_t channel) {
    return (channel & 7) << 4;
}

uint8_t MCP3008::control_bits(uint8_t channel) {
    return 0x8 | control_bits_differential(channel);
}

int MCP3008::reading(int channel)
{
    uint8_t tx[] = {1, control_bits(channel), 0};
    uint8_t rx[3];
    struct spi_ioc_transfer tr;
    
    if (!m_enabled)
        return 0;
    
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = ARRAY_SIZE(tx),
    tr.delay_usecs = DELAY;
    tr.speed_hz = CLOCK;
    tr.bits_per_word = BITS;

    if (ioctl(m_fd, SPI_IOC_MESSAGE(1), &tr) == 1) {
        syslog(LOG_ERR, "ioctl: SPI_IOC_MESSAGE %s(%d)", strerror(errno), errno);
        std::cerr << __FUNCTION__ << ": ioctl: SPI_IOC_MESSAGE " << strerror(errno) << errno << std::endl;
        return 0;
    }

    return ((rx[1] << 8) & 0x300) | (rx[2] & 0xFF);
}

