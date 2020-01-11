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

#ifndef DS18B20_H
#define DS18B20_H

#include <string>
#include <map>
#include <vector>
#include <syslog.h>
#include <sys/types.h>
#include <dirent.h>

#include "temperature.h"

class DS18B20
{
public:
    DS18B20();
    ~DS18B20();
    
    int count() { return m_count; }
    double celsiusReadingByName(std::string &name);
    double celsiusReadingByIndex(int index);
    double farenheitReadingByName(std::string &name);
    double farenheitReadingByIndex(int index);
    std::map<std::string, double> celsius();
    std::map<std::string, double> farenheit();
    std::map<std::string, std::string> devices();
    void setDeviceName(std::string &device, std::string name);
    
private:
    std::map<int, Temperature*> m_devices;
    int m_index;
    int m_count;
};

#endif // DS18B20_H
