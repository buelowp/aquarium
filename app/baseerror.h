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

#ifndef BASEERROR_H
#define BASEERROR_H

#include <string>
#include <gpiointerruptpp.h>

#include "mqttclient.h"
#include "configuration.h"

/**
 * \class BaseError
 * 
 * Provides a container for a priority_queue to keep track of what
 * errors/warnings are currently active in the system. Priority is set
 * by PriorityType and will function as the following
 * 
 * FATAL: Cannot be cleared, will remain active forever
 * CRITICAL: Can be cleared, but turns on red LED
 * WARNING: Can be cleared, turns on yellow LED
 * 
 * Errors are pushed into the queue by the priority level with
 * warning being the lowest priority.
 */
class BaseError
{
public:
    typedef enum PRIORITYTYPE: int {
        WARNING = 0,
        CRITICAL = 1,
        FATAL = 2
    } Priority;
    
    BaseError(unsigned int handle, std::string msg, unsigned int timeout = 0);
    virtual ~BaseError();

    int priority() const { return m_priority; }
    int handle() const { return m_handle; }
    
    bool operator<(const BaseError &lhs, const BaseError &rhs)
    {
        return (lhs.priority() < rhs.priority());
    }
    
protected:
    MQTTClient *m_mqtt;
    Priority m_priority;
    std::string m_message;
    unsigned int m_timeout;
    int m_handle;
};

#endif // BASEERROR_H
