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

#include "temperature.h"

Temperature::Temperature()
{
    m_enabled = false;
    std::vector<std::string> v;
    
    DIR* dirp = opendir("/sys/bus/w1/devices/");
    struct dirent * dp;
    while ((dp = readdir(dirp)) != NULL) {
        v.push_back(dp->d_name);
    }
    closedir(dirp);
    
    for (int i = 0; i < v.size(); i++) {
        if (v.at(i).find("28-") != std::string::npos) {
            m_devices[v.at(i)] = v.at(i);
            syslog(LOG_INFO, "Found DS18B20 device %s", v.at(i).c_str());
            std::cout << "Found DS18B20 device " << v.at(i).c_str() << std::endl;
        }
    }
    if (m_devices.size() > 0)
        m_enabled = true;
    else
        syslog(LOG_ERR, "No 1-wire devices found");
    
    std::cout << __FUNCTION__ << ": Found " << m_devices.size() << " devices" << std::endl;
}

Temperature::Temperature(std::string name, std::string device)
{
    m_devices[device] = name;
    m_enabled = true;
}

Temperature::~Temperature()
{
}

double Temperature::getTemperatureByName(std::string name)
{
    auto it = m_devices.begin();
    
    while (it != m_devices.end()) {
        if (it->second == name)
            return getTemperature(it->first);
    }
    return 0;
}

double Temperature::getTemperatureByDevice(std::string device)
{
    auto it = m_devices.find(device);
    
    if (it != m_devices.end()) {
        return getTemperature(device);
    }
    return 0;
}

double Temperature::getTemperature(std::string device)
{
    std::string path = "/sys/bus/w1/devices/" + device + "/w1_slave";
    std::ifstream fs(path);
    std::stringstream contents;
    std::string data;
        
    if (fs.is_open() && m_enabled) {
        contents << fs.rdbuf();
        int pos = contents.str().find("t=");
        if (pos != std::string::npos) {
            data = contents.str().substr(pos + 2, 5);
        }
        try {
            double t = std::stof(data);
            return (t / 1000);
        }
        catch (std::exception &e) {
            syslog(LOG_ERR, "Unable to decode %s, exception is %s\n", data.c_str(), e.what());
            std::cerr << __FUNCTION__ << "Unable to decode " << data << std::endl;
        }
    }
    return 0;
}

bool Temperature::setNameForDevice(std::string name, std::string device)
{
    auto it = m_devices.find(device);
    
    if (it != m_devices.end()) {
        m_devices[device] = name;
        return true;
    }
    return false;
}


