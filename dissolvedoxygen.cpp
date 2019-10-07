/*
 * Copyright (c) 2019 <copyright holder> <email>
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

#include "dissolvedoxygen.h"

DissolvedOxygen::DissolvedOxygen(uint8_t device, uint8_t address) : AtlasScientificI2C(device, address)
{
    m_enabled = true;
}

DissolvedOxygen::~DissolvedOxygen()
{
}

void DissolvedOxygen::response(int cmd, uint8_t *buffer, int size)
{
    std::string r;
    std::vector<std::string> result;
    
    for (int i = 0; i < size; i++) {
        r += static_cast<char>(buffer[i]);
    }
    
    switch (cmd) {
    case AtlasScientificI2C::INFO:
        result = split(r, ',');
        if (result.size() == 3) {
            m_version = result[2];
            if (result[1] != "DO") {
                m_enabled = false;
                syslog(LOG_ERR, "Attempted to enable DO sensor, but reply was from a %s sensor", result[1].c_str());
                syslog(LOG_ERR, "Reply from sensor confused me: %s", r.c_str());
            }
            else {
                syslog(LOG_INFO, "DO Sensor is enabled with sensor version %s", m_version.c_str());
                std::cout << "DO Sensor is enabled with sensor version " << m_version << std::endl;
                m_enabled = true;
            }
        }
        else {
            syslog(LOG_ERR, "Reply from sensor confused me: %s", r.c_str());
            m_enabled = false;
        }
        break;
    case AtlasScientificI2C::CALIBRATE:
        handleCalibration(r);
        break;
    case AtlasScientificI2C::STATUS:
        handleStatusResponse(r);
        break;
    default:
        break;
    }

    try {
        m_callback(cmd, r);
    }
    catch (const std::bad_function_call& e) {
        syslog(LOG_ERR, "exception executing callback function: %s\n", e.what());
    }
}

void DissolvedOxygen::getLastResponse(std::string &r)
{
    r.clear();
    
    for (int i = 0; i < m_lastResponse.size(); i++) {
        r += static_cast<char>(m_lastResponse[i]);
    }
}

void DissolvedOxygen::handleCalibration(std::string response)
{
    std::vector<std::string> result;
    
    result = split(response, ',');
    
    if (result.size() == 0) {
        syslog(LOG_INFO, "Calibration event accepted");
        return;
    }
    else if (result.size() == 2) {
        if (result[0] != "?CAL") {
            m_calibration = 0;
            syslog(LOG_ERR, "Reply from sensor confused me: %s", response.c_str());
        }
        else {
            try {
                m_calibration = std::stoi(result.at(1));
            }
            catch (std::exception &e) {
                syslog(LOG_ERR, "Calibration query returned a non number: %s", response.c_str());
            }
            syslog(LOG_INFO, "Device has %d point calibration", m_calibration);
            std::cout << "Device has " << m_calibration << " point calibration" << std::endl;
        }
    }
    else {
        syslog(LOG_ERR, "Reply from sensor confused me: %s", response.c_str());
        m_enabled = false;
    }
}

void DissolvedOxygen::handleStatusResponse(std::string response)
{
    std::vector<std::string> results = split(response, ',');
    
    if (results.size() == 3) {
        if (results[0] != "?STATUS") {
            m_lastVoltage = 0.0;
            m_lastResetReason = 'U';
            syslog(LOG_ERR, "Reply from sensor confused me: %s", response.c_str());
        }
        else {
            try {
                m_lastVoltage = std::stof(results[2]);
                m_lastResetReason = results[1];
            }
            catch (std::exception &e) {
                syslog(LOG_ERR, "Status query returned a non number: %s", response.c_str());                
            }
        }
    }
    else {
        m_lastVoltage = 0.0;
        m_lastResetReason = 'U';
        syslog(LOG_ERR, "Reply from sensor confused me: %s", response.c_str());
    }
}
