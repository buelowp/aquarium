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

/**
 * \fn ErrorHandler::ErrorHandler()
 * 
 * Set m_handle to 100 because we want to reserve some values
 * for static assignment, these would be known errors
 */
ErrorHandler::ErrorHandler()
{
    m_storedErrors = 0;
    m_handle = 100;
}

ErrorHandler::~ErrorHandler()
{
}

unsigned int ErrorHandler::critical(std::string msg, mqtt::async_client *client, int timeout, int handle)
{
    if (handle > 0)
        m_handle = handle;
    else
        m_handle++;
    
    Critical err(m_handle, msg, client, timeout);
    
    err.activate();
    
    digitalWrite(Configuration::instance()->m_greenLed, 0);
    m_criticals[m_handle] = err;
    m_storedErrors++;
    return m_handle;
}

unsigned int ErrorHandler::fatal(std::string msg, mqtt::async_client *client, int handle)
{
    if (handle > 0)
        m_handle = handle;
    else
        m_handle++;
    
    Fatal err(m_handle, msg, client);
    err.activate();
    
    digitalWrite(Configuration::instance()->m_greenLed, 0);
    m_fatals[m_handle] = err;
    m_storedErrors++;
    return m_handle;
}

unsigned int ErrorHandler::warning(std::string msg, mqtt::async_client *client, int timeout, int handle)
{
    if (handle > 0)
        m_handle = handle;
    else
        m_handle++;
    
    Warning err(m_handle, msg, client, timeout);
    
    err.activate();
    
    digitalWrite(Configuration::instance()->m_greenLed, 0);
    m_warnings[m_handle] = err;
    m_storedErrors++;
    return m_handle;
}

void ErrorHandler::clearCritical(unsigned int handle)
{
    auto search = m_criticals.find(handle);
    if (search != m_criticals.end()) {
        Critical err = search->second;
        err.cancel();
        m_criticals.erase(search);
        m_storedErrors--;
    }
    
    if (m_criticals.size() == 0) {
        if (m_warnings.size()) {
            auto it = m_warnings.begin();
            Warning err = it->second;
            err.activate();
        }
    }
    else {
        auto it = m_criticals.begin();
        Critical err = it->second;
        err.activate();
    }
    if (m_storedErrors == 0) {
        digitalWrite(Configuration::instance()->m_greenLed, 1);
    }
}

void ErrorHandler::clearWarning(unsigned int handle)
{
    auto search = m_warnings.find(handle);
    if (search != m_warnings.end()) {
        Warning err = search->second;
        err.cancel();
        m_warnings.erase(search);
        m_storedErrors--;
    }
    if (m_storedErrors == 0) {
        digitalWrite(Configuration::instance()->m_greenLed, 1);
    }

}
