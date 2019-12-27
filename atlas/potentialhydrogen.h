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

#ifndef POTENTIALHYDROGEN_H__
#define POTENTIALHYDROGEN_H__

#include <string>
#include <functional>
#include <iostream>
#include <iterator>
#include <iomanip>

#include "atlasscientifici2c.h"

/**
 * @todo write docs
 */
class PotentialHydrogen : public AtlasScientificI2C
{
public:
    static const int CLEAR = 100;
    static const int LOW = 101;
    static const int MID = 102;
    static const int HIGH = 103;
    static const int QUERY = 104;
    
    static const int NOCALIBRATION = 0;
    static const int ONEPOINTCAL = 1;
    static const int TWOPOINTCAL = 2;
    static const int THREEPOINTCAL = 3;
    
    PotentialHydrogen(uint8_t, uint8_t);
    virtual ~PotentialHydrogen();
    
    void getLastResponse(std::string&);
    void setCallback(std::function<void(int, std::string)> cbk) { m_callback = cbk; }

    void response(int cmd, uint8_t[], int) override;

    void calibrate(int, uint8_t*, int);
    void slope();
    void setTempCompensation(uint8_t*, int);
    void setTempCompensationAndRead(uint8_t*, int);
    void getTempCompensation();
    std::string getLastReason() { return m_lastResetReason; }
    double getVoltage() { return m_lastVoltage; }
    double getPH() { return m_lastPHValue; }
    
private:
    void handleCalibration(std::string);
    void handleStatusResponse(std::string);
    void handleReadResponse(std::string&);
    
    std::function<void(int, std::string)> m_callback;
    int m_calibration;
    std::string m_lastResetReason;
    double m_lastVoltage;
    double m_lastPHValue;
};

#endif // DISSOLVEDOXYGEN_H
