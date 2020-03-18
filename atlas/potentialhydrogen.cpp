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

#include "potentialhydrogen.h"

PotentialHydrogen::PotentialHydrogen(uint8_t device, uint8_t address) : AtlasScientificI2C(device, address)
{
    m_calibration = 0;
    m_lastVoltage = 0.0;
}

PotentialHydrogen::~PotentialHydrogen()
{
}

void PotentialHydrogen::response(int cmd, uint8_t *buffer, int size)
{
    std::string r;
    std::vector<std::string> result;
    std::string ph("pH");
    
    if (!m_enabled)
        return;
    
    for (int i = 0; i < size; i++) {
        r += static_cast<char>(buffer[i]);
    }
    
    switch (cmd) {
    case AtlasScientificI2C::INFO:
        result = split(r, ',');
        if (result.size() == 3) {
            m_version = result[2];
            if (result[1] != ph) {
                m_enabled = false;
                syslog(LOG_ERR, "%s:%d: Attempted to enable pH sensor, but reply was from a %s sensor", __FUNCTION__, __LINE__, result[1].c_str());
                syslog(LOG_ERR, "%s:%d: Reply from sensor confused me: %s", __FUNCTION__, __LINE__, r.c_str());
            }
            else {
                m_enabled = true;
            }
        }
        else {
            syslog(LOG_ERR, "%s:%d: Reply from sensor confused me: %s", __FUNCTION__, __LINE__, r.c_str());
            m_enabled = false;
        }
        break;
    case AtlasScientificI2C::CALIBRATE:
        handleCalibration(r);
        break;
    case AtlasScientificI2C::STATUS:
        handleStatusResponse(r);
        break;
    case AtlasScientificI2C::READING:
        handleReadResponse(r);
        break;
    default:
        break;
    }

    try {
        m_callback(cmd, r);
    }
    catch (const std::bad_function_call& e) {
        syslog(LOG_ERR, "%s:%d: exception executing callback function: %s\n", __FUNCTION__, __LINE__, e.what());
        fprintf(stderr, "%s:%d: exception executing callback function: %s\n", __FUNCTION__, __LINE__, e.what());
    }
}

void PotentialHydrogen::getLastResponse(std::string &r)
{
    if (!m_enabled)
        return;
    
    r.clear();
    
    for (int i = 0; i < m_lastResponse.size(); i++) {
        r += static_cast<char>(m_lastResponse[i]);
    }
}

bool PotentialHydrogen::calibrate(int cmd)
{
    if (!m_enabled)
        return false;
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << m_lastPHValue;
    std::string val = ss.str();
    uint8_t buf[16] = {0};
    
    for (int i = 0; i < val.size(); i++) {
        buf[i] = val[i];
    }
    calibrate(cmd, buf, val.size());
    return true;
}

void PotentialHydrogen::printBuffer(std::vector<uint8_t> &packet)
{
    if (!m_enabled)
        return;
    
    std::ios oldState(nullptr);
    
    oldState.copyfmt(std::cout);
    std::cout << "Packet: ";
    for (int i = 0; i < packet.size(); i++) {
        std::cout << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(packet[i]) << " ";
    }
    std::cout << std::endl;
    std::cout.copyfmt(oldState);
}

bool PotentialHydrogen::calibrate(int cmd, uint8_t buf[], int size)
{
    std::vector<uint8_t> payload;
    
    if (!m_enabled)
        return false;
    
    payload.push_back('C');
    payload.push_back('a');
    payload.push_back('l');
    payload.push_back(',');
    
    switch (cmd) {
        case LOW:
            payload.push_back('l');
            payload.push_back('o');
            payload.push_back('w');
            payload.push_back(',');
            for (int i = 0; i < size; i++) {
                payload.push_back(buf[i]);
            }
            printBuffer(payload);
            std::cout << "Setting LOW calibration value to " << m_lastPHValue << std::endl;
//            sendCommand(AtlasScientificI2C::CALIBRATE, payload.data(), payload.size(), 900);
            break;
        case MID:
            payload.push_back('m');
            payload.push_back('i');
            payload.push_back('d');
            payload.push_back(',');
            for (int i = 0; i < size; i++) {
                payload.push_back(buf[i]);
            }
            printBuffer(payload);
            std::cout << "Setting MID calibration value to " << m_lastPHValue << std::endl;
//            sendCommand(AtlasScientificI2C::CALIBRATE, payload.data(), payload.size(), 900);
            break;
        case HIGH:
            payload.push_back('h');
            payload.push_back('i');
            payload.push_back('g');
            payload.push_back('h');
            payload.push_back(',');
            for (int i = 0; i < size; i++) {
                payload.push_back(buf[i]);
            }
            printBuffer(payload);
            std::cout << "Setting HIGH calibration value to " << m_lastPHValue << std::endl;
//            sendCommand(AtlasScientificI2C::CALIBRATE, payload.data(), payload.size(), 900);
            break;
        case CLEAR:
            payload.push_back('c');
            payload.push_back('l');
            payload.push_back('e');
            payload.push_back('a');
            payload.push_back('r');
            printBuffer(payload);
//            sendCommand(AtlasScientificI2C::CALIBRATE, payload.data(), payload.size(), 300);
            break;
        case QUERY:
            payload.push_back('?');
            sendCommand(AtlasScientificI2C::CALIBRATE, payload.data(), payload.size(), 300);
            break;
        default:
            break;
    }
    return true;
}

void PotentialHydrogen::slope()
{
    std::vector<uint8_t> payload;
    
    if (!m_enabled)
        return;
    
    payload.push_back('S');
    payload.push_back('l');
    payload.push_back('o');
    payload.push_back('p');
    payload.push_back('e');
    payload.push_back(',');
    payload.push_back('?');
    sendCommand(AtlasScientificI2C::SLOPE, payload.data(), payload.size(), 300);
}

void PotentialHydrogen::setTempCompensation(double temp)
{
    if (!m_enabled)
        return;
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << temp;
    std::string val = ss.str();
    uint8_t buf[16] = {0};
    
    for (int i = 0; i < val.size(); i++) {
        buf[i] = val[i];
    }
    setTempCompensation(buf, val.size());
}

void PotentialHydrogen::setTempCompensation(uint8_t *buf, int size)
{
    std::vector<uint8_t> payload;
    
    if (!m_enabled)
        return;
    
    payload.push_back('T');
    payload.push_back(',');
    
    for (int i = 0; i < size; i++) {
        payload.push_back(buf[i]);
    }
    sendCommand(AtlasScientificI2C::SETTEMPCOMP, payload.data(), payload.size(), 300);
}

void PotentialHydrogen::setTempCompensationAndRead(double temp)
{
    if (!m_enabled)
        return;
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << temp;
    std::string val = ss.str();
    uint8_t buf[16] = {0};
    
    for (int i = 0; i < val.size(); i++) {
        buf[i] = val[i];
    }
    setTempCompensationAndRead(buf, val.size());
}

void PotentialHydrogen::setTempCompensationAndRead(uint8_t *buf, int size)
{
    std::vector<uint8_t> payload;
    
    if (!m_enabled)
        return;
    
    payload.push_back('R');
    payload.push_back('T');
    payload.push_back(',');
    
    for (int i = 0; i < size; i++) {
        payload.push_back(buf[i]);
    }
    sendCommand(AtlasScientificI2C::SETTEMPCOMPREAD, payload.data(), payload.size(), 900);
}

void PotentialHydrogen::getTempCompensation()
{
    std::vector<uint8_t> payload;
    
    if (!m_enabled)
        return;
    
    payload.push_back('T');
    payload.push_back(',');
    payload.push_back('?');
    
    sendCommand(AtlasScientificI2C::GETTEMPCOMP, payload.data(), payload.size(), 300);
}

void PotentialHydrogen::handleCalibration(std::string response)
{
    std::vector<std::string> result;
    std::string cal("?CAL");
    
    if (!m_enabled)
        return;
    
    result = split(response, ',');
    
    if (result.size() == 0) {
        syslog(LOG_INFO, "%s:%d: Calibration event accepted", __FUNCTION__, __LINE__);
        return;
    }
    else if (result.size() == 2) {
        if (result[0] != cal) {
            m_calibration = 0;
            syslog(LOG_ERR, "%s:%d: Reply from sensor confused me: %s", __FUNCTION__, __LINE__, response.c_str());
        }
        else {
            try {
                m_calibration = std::stoi(result.at(1));
            }
            catch (std::exception &e) {
                syslog(LOG_ERR, "%s:%d: Calibration query returned a non number: %s", __FUNCTION__, __LINE__, response.c_str());
                fprintf(stderr, "%s:%d: Calibration query returned a non number: %s", __FUNCTION__, __LINE__, response.c_str());
            }
            syslog(LOG_INFO, "%s:%d: Device has %d point calibration", __FUNCTION__, __LINE__, m_calibration);
        }
    }
    else {
        syslog(LOG_ERR, "%s:%d: Reply from sensor confused me: %s", __FUNCTION__, __LINE__, response.c_str());
        m_enabled = false;
    }
}

void PotentialHydrogen::handleStatusResponse(std::string response)
{
    std::vector<std::string> results = split(response, ',');
    std::string status("?STATUS");
    
    if (!m_enabled)
        return;
    
    if (results.size() == 3) {
        if (results[0] == status) {
            try {
                m_lastVoltage = std::stof(results[2]);
                m_lastResetReason = results[1];
            }
            catch (std::exception &e) {
                syslog(LOG_ERR, "%s:%d: Status query returned a non number: %s", __FUNCTION__, __LINE__, response.c_str());                
                fprintf(stderr, "%s:%d: Status query returned a non number: %s", __FUNCTION__, __LINE__, response.c_str());                
            }
        }
        else {
            m_lastVoltage = 0.0;
            m_lastResetReason = 'U';
            syslog(LOG_ERR, "%s:%d: Reply from sensor confused me: .%s.", __FUNCTION__, __LINE__, results[0].c_str());
        }
    }
    else {
        m_lastVoltage = 0.0;
        m_lastResetReason = 'U';
        syslog(LOG_ERR, "%s:%d: Reply from sensor confused me: %s", __FUNCTION__, __LINE__, response.c_str());
    }
}

void PotentialHydrogen::handleReadResponse(std::string &response)
{
    if (!m_enabled)
        return;
    
    response.erase(response.begin());
    try {
        m_lastPHValue = std::stod(response);
    }
    catch (std::exception &e) {
        std::cerr << __FUNCTION__ << "Unable to decode response: " << response << std::endl;
    }
}
