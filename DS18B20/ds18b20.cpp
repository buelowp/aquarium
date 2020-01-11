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

#include "ds18b20.h"

DS18B20::DS18B20() : m_count(0)
{
    std::vector<std::string> v;
    DIR* dirp = opendir("/sys/bus/w1/devices/");
    struct dirent * dp;
    
    while ((dp = readdir(dirp)) != NULL) {
        v.push_back(dp->d_name);
    }
    closedir(dirp);
    
    for (int i = 0; i < v.size(); i++) {
        if (v.at(i).find("28-") != std::string::npos) {
            Temperature *temp = new Temperature(v.at(i));
            m_devices.insert(std::pair<int, Temperature*>(m_count++, temp));
        }
    }
    
    m_index = 0;
}

DS18B20::~DS18B20()
{
}

double DS18B20::celsiusReadingByIndex(int index)
{
    if (m_devices.size() && (m_count > index)) {
        Temperature *temp = m_devices.at(index);
        if (temp) {
            return temp->celsius();
        }
    }
    return 0;
}

double DS18B20::celsiusReadingByName(std::string& name)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (it->second->name() == name) {
            return it->second->celsius();
        }
    }
    return 0; 
}

double DS18B20::farenheitReadingByIndex(int index)
{
    if (m_devices.size() && (m_count > index)) {
        Temperature *temp = m_devices.at(index);
        if (temp) {
            return temp->farenheit();
        }
    }
    return 0;
}


double DS18B20::farenheitReadingByName(std::string& name)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (it->second->name() == name) {
            return it->second->farenheit();
        }
    }
    return 0; 
}

void DS18B20::setDeviceName(std::string& device, std::string name)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (it->second->device() == device) {
            it->second->setName(name);
        }
    }
}

std::map<std::string, double> DS18B20::celsius()
{
    std::map<std::string, double> readings;
    
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        readings.insert(std::pair<std::string, double>(it->second->name(), it->second->celsius()));
    }
    return readings;
}

std::map<std::string, double> DS18B20::farenheit()
{
    std::map<std::string, double> readings;
    
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        readings.insert(std::pair<std::string, double>(it->second->name(), it->second->farenheit()));
    }
    return readings;
}




