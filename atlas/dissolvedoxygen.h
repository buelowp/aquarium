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

#ifndef DISSOLVEDOXYGEN_H
#define DISSOLVEDOXYGEN_H

#include <iostream>
#include <functional>
#include <string>
#include <iomanip>

#include <syslog.h>

#include "atlasscientifici2c.h"

class DissolvedOxygen : public AtlasScientificI2C
{
public:
    static const int DO_CLEAR = 100;
    static const int DO_DEFAULT = 101;
    static const int DO_LOW = 102;
    static const int DO_ZERO = 102;
    static const int DO_QUERY = 104;
 
    DissolvedOxygen(uint8_t, uint8_t);
    virtual ~DissolvedOxygen();

    void getLastResponse(std::string&);
    void setCallback(std::function<void(int, std::string)> cbk) { m_callback = cbk; }
    void calibrate(int, uint8_t*, int);
    void response(int cmd, uint8_t*, int) override;
    double getDO() { return m_lastDOValue; }
    void setTempCompensation(uint8_t*, int);
    void setTempCompensation(double);
    void setTempCompensationAndRead(uint8_t*, int);
    void setTempCompensationAndRead(double);
    void getTempCompensation();

private:
    void handleCalibration(std::string);
    void handleStatusResponse(std::string);
    void handleReadResponse(std::string&);
    void printBuffer(std::vector<uint8_t>&);
    
    std::function<void(int, std::string)> m_callback;
    int m_calibration;
    std::string m_lastResetReason;
    double m_lastVoltage;
    double m_lastDOValue;
};

#endif // DISSOLVEDOXYGEN_H
