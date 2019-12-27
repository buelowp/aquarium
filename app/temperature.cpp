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
            m_device = v.at(i);
            break;
        }
    }
    if (m_device.size() > 0) {
        m_enabled = true;
    
        m_path = "/sys/bus/w1/devices/" + m_device + "/w1_slave";
        syslog(LOG_INFO, "Found 1-wire device %s", m_path.c_str());
    }
    else
        syslog(LOG_ERR, "No 1-wire devices found");
}

Temperature::Temperature(std::string device) : m_device(device)
{
    m_path = "/sys/bus/w1/devices/" + m_device + "/w1_slave";
    m_enabled = true;
}

Temperature::~Temperature()
{
}

void Temperature::getTemperature(float &tc, float &tf)
{
    std::ifstream fs(m_path);
    std::stringstream contents;
    std::string data;
    
    if (fs.is_open() && m_enabled) {
        contents << fs.rdbuf();
        int pos = contents.str().find("t=");
        if (pos != std::string::npos) {
            data = contents.str().substr(pos + 2, 5);
        }
        try {
            float t = std::stof(data);
            tc = t / 1000;
            tf = (tc * 1.8) + 32;
        }
        catch (std::exception &e) {
            syslog(LOG_ERR, "Unable to decode %s, exception is %s\n", data.c_str(), e.what());
            std::cerr << "Unable to decode " << data << std::endl;
        }
    }
}

