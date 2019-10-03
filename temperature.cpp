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

Temperature::Temperature(std::string device) : m_device(device)
{
    m_path = "/sys/bus/w1/devices/" + m_device + "w1_slave";
}

Temperature::~Temperature()
{
}

void Temperature::getTemperature(double &temp)
{
    std::ifstream fs(m_path);
    std::string data;
    
    if (fs.is_open()) {
        std::stringstream contents;
        contents << fs.rdbuf();
        int pos = contents.str().find("t=");
        if (pos != std::string::npos) {
            while (contents.str().at(pos) != '\n') {
                data += contents.str().at(pos++);
            }
        }
    }
    
    try {
        temp = std::stod(data);
    }
    catch (std::exception &e) {
        syslog(LOG_ERR, "Unable to decode %s, exception is %s\n", data.c_str(), e.what());
    }
}

