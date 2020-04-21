/*
 * Copyright (c) 2020 <copyright holder> <email>
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

#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <vector>

#include <wiringPi.h>

#include "configuration.h"
#include "warning.h"
#include "critical.h"
#include "fatal.h"

class ErrorHandler
{
public:
    ErrorHandler();
    ~ErrorHandler();
    
    unsigned int fatal(unsigned int handle, std::string msg);
    unsigned int critical(unsigned int handle, std::string msg);
    unsigned int warning(unsigned int handle, std::string msg);
    
    void clearFatal(unsigned int handle);
    void clearCritical(unsigned int handle);
    void clearWarning(unsigned int handle);

private:
    std::map<int, Critical> m_criticals;
    std::map<int, Fatal> m_fatals;
    std::map<int, Warning> m_warnings;
    int m_storedErrors;
};

#endif // ERRORHANDLER_H
