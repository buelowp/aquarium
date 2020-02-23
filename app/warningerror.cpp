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

#include "warningerror.h"

WarningError::WarningError(unsigned int handle, std::string msg, unsigned int timeout) : BaseError(handle, msg, timeout)
{
    m_priority = BaseError::Priority::WARNING;
    nlohmann::json j;
    
    j["aquarium"]["error"]["type"] = "warning";
    j["aquarium"]["error"]["message"] = msg;
    j["aquarium"]["error"]["handle"] = handle;
    j["aquarium"]["error"]["timeout"] = timeout;
    
    m_mqtt->publish(NULL, "aquarium/error", j.dump().size(), j.dump().c_str());
    GpioInterrupt::instance()->setValue(Configuration::instance()->m_yellow_led, 1);
}

WarningError::~WarningError()
{
    nlohmann::json j;
    
    j["aquarium"]["error"]["type"] = "warning";
    j["aquarium"]["error"]["message"] = "cleared";
    j["aquarium"]["error"]["handle"] = handle;
    j["aquarium"]["error"]["timeout"] = 0;
    
    m_mqtt->publish(NULL, "aquarium/error", j.dump().size(), j.dump().c_str());
    GpioInterrupt::instance()->setValue(Configuration::instance()->m_yellow_led, 0);
}
