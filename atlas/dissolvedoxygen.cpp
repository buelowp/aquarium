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
    std::string DO("DO");
    
    for (int i = 0; i < size; i++) {
        r += static_cast<char>(buffer[i]);
    }
    
    switch (cmd) {
    case AtlasScientificI2C::INFO:
        result = split(r, ',');
        if (result.size() == 3) {
            m_version = result[2];
            if (result[1] != DO) {
                m_enabled = false;
                syslog(LOG_ERR, "%s:%d: Attempted to enable DO sensor, but reply was from a %s sensor", __FUNCTION__, __LINE__, result[1].c_str());
                syslog(LOG_ERR, "Reply from sensor confused me: %s", r.c_str());
            }
            else {
                syslog(LOG_INFO, "%s:%d: DO Sensor is enabled with sensor version %s", __FUNCTION__, __LINE__, m_version.c_str());
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
    std::string cal("?CAL");
    
    result = split(response, ',');
    
    if (result.size() == 0) {
        syslog(LOG_INFO, "Calibration event accepted");
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

void DissolvedOxygen::handleStatusResponse(std::string response)
{
    std::vector<std::string> results = split(response, ',');
    std::string status("?STATUS");

    if (results.size() == 3) {
        if (results[0] != status) {
            m_lastVoltage = 0.0;
            m_lastResetReason = 'U';
            syslog(LOG_ERR, "%s:%d: Reply from sensor confused me: %s", __FUNCTION__, __LINE__, response.c_str());
            std::cout << "Error comparing " << results[0] << " and " << status << ", results[0] size is " << results[0].size() << std::endl;
        }
        else {
            try {
                m_lastVoltage = std::stof(results[2]);
                m_lastResetReason = results[1];
            }
            catch (std::exception &e) {
                syslog(LOG_ERR, "%s:%d: Status query returned a non number: %s", __FUNCTION__, __LINE__, response.c_str());                
                fprintf(stderr, "%s:%d: Status query returned a non number: %s", __FUNCTION__, __LINE__, response.c_str());                
            }
        }
    }
    else {
        m_lastVoltage = 0.0;
        m_lastResetReason = 'U';
        syslog(LOG_ERR, "%s:%d: Reply from sensor confused me: %s", __FUNCTION__, __LINE__, response.c_str());
    }
}

void DissolvedOxygen::calibrate(int cmd, uint8_t buf[], int size)
{
    std::vector<uint8_t> payload;
    
    payload.push_back('C');
    payload.push_back('a');
    payload.push_back('l');
    payload.push_back(',');
    
    switch (cmd) {
        case DEFAULT:
            payload.pop_back();
            printBuffer(payload);
//            sendCommand(AtlasScientificI2C::CALIBRATE, payload.data(), payload.size(), 1300);
            break;
        case ZERO:
            payload.push_back('0');
            sendCommand(AtlasScientificI2C::CALIBRATE, payload.data(), payload.size(), 1300);
            break;            
        case CLEAR:
            payload.push_back('c');
            payload.push_back('l');
            payload.push_back('e');
            payload.push_back('a');
            payload.push_back('r');
            sendCommand(AtlasScientificI2C::CALIBRATE, payload.data(), payload.size(), 300);
            break;
        case QUERY:
            payload.push_back('?');
            sendCommand(AtlasScientificI2C::CALIBRATE, payload.data(), payload.size(), 300);
            break;
        default:
            break;
    }
}

void DissolvedOxygen::handleReadResponse(std::string &response)
{
    response.erase(response.begin());
    try {
        m_lastDOValue = std::stod(response);
    }
    catch (std::exception &e) {
        std::cerr << __FUNCTION__ << "Unable to decode response: " << response << std::endl;
    }
}

void DissolvedOxygen::printBuffer(std::vector<uint8_t> &packet)
{
    std::ios oldState(nullptr);
    
    oldState.copyfmt(std::cout);
    std::cout << "Packet: ";
    for (int i = 0; i < packet.size(); i++) {
        std::cout << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(packet[i]) << " ";
    }
    std::cout << std::endl;
    std::cout.copyfmt(oldState);
}

void DissolvedOxygen::setTempCompensation(double temp)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << temp;
    std::string val = ss.str();
    uint8_t buf[16] = {0};
    
    for (int i = 0; i < val.size(); i++) {
        buf[i] = val[i];
    }
    setTempCompensation(buf, val.size());
}

void DissolvedOxygen::setTempCompensation(uint8_t *buf, int size)
{
    std::vector<uint8_t> payload;
    
    payload.push_back('T');
    payload.push_back(',');
    
    for (int i = 0; i < size; i++) {
        payload.push_back(buf[i]);
    }
    sendCommand(AtlasScientificI2C::SETTEMPCOMP, payload.data(), payload.size(), 300);
}

void DissolvedOxygen::setTempCompensationAndRead(double temp)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << temp;
    std::string val = ss.str();
    uint8_t buf[16] = {0};
    
    for (int i = 0; i < val.size(); i++) {
        buf[i] = val[i];
    }
    setTempCompensationAndRead(buf, val.size());
}

void DissolvedOxygen::setTempCompensationAndRead(uint8_t *buf, int size)
{
    std::vector<uint8_t> payload;
    
    payload.push_back('R');
    payload.push_back('T');
    payload.push_back(',');
    
    for (int i = 0; i < size; i++) {
        payload.push_back(buf[i]);
    }
    sendCommand(AtlasScientificI2C::SETTEMPCOMPREAD, payload.data(), payload.size(), 900);
}

void DissolvedOxygen::getTempCompensation()
{
    std::vector<uint8_t> payload;
    
    payload.push_back('T');
    payload.push_back(',');
    payload.push_back('?');
    
    sendCommand(AtlasScientificI2C::GETTEMPCOMP, payload.data(), payload.size(), 300);
}

