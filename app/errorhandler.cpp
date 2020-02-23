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

#include "errorhandler.h"

ErrorHandler::ErrorHandler()
{
}

ErrorHandler::~ErrorHandler()
{
}

unsigned int ErrorHandler::critical (unsigned int handle, std::string msg)
{
    CriticalError err(handle, msg);
    
    if (m_fatals.size() == 0)
        err.activate();
    
    m_criticals[handle] = err;
}

unsigned int ErrorHandler::fatal ( unsigned int handle, std::string msg )
{
    FatalError err(handle, msg);
    
    m_fatals[handle] = err;
}

unsigned int ErrorHandler::warning ( unsigned int handle, std::string msg )
{
    WarningError err(handle, msg);
    
    if (m_fatals.size() == 0 && m_criticals.size() == 0)
        err.activate();
    
    m_warnings[handle] = err;
}

void ErrorHandler::clearCritical ( unsigned int handle )
{
    auto search = m_criticals.find(handle);
    if (search != m_criticals.end()) {
        CriticalError err = search->second;
        err.cancel();
        m_criticals.erase(search);
    }
}

void ErrorHandler::clearFatal ( unsigned int handle )
{
    auto search = m_fatals.find(handle);
    if (search != m_fatals.end()) {
        FatalError err = search->second;
        err.cancel();
        m_fatals.erase(search);
    }
}

void ErrorHandler::clearWarning ( unsigned int handle )
{
    auto search = m_warnings.find(handle);
    if (search != m_warnings.end()) {
        WarningError err = search->second;
        err.cancel();
        m_warnings.erase(search);
    }
}
