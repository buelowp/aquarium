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

#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <syslog.h>
#include <sys/types.h>
#include <dirent.h>

class Temperature
{
public:
    Temperature();
    Temperature(std::string);
    ~Temperature();
    
    void getTemperature(double&, double&);
    double celsius();
    double farenheit();
    bool enabled() { return m_enabled; }
    std::string name() { return m_name; }
    std::string device() { return m_device; }
    void setName(std::string name) { m_name = name; }
    
private:
    std::string m_device;
    std::string m_path;
    std::string m_name;
    bool m_enabled;
};

#endif // TEMPERATURE_H
