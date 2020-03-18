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

#include "critical.h"

Critical::Critical()
{
    m_handle = 0;
    m_timeout = 0;
    m_priority = BaseError::Priority::CRITICAL;
}

Critical::Critical(const Critical& ce)
{
    m_priority = ce.priority();
    m_message = ce.message();
    m_timeout = ce.timeout();
    m_handle = ce.handle();
    m_mqtt = ce.client();
}

Critical::Critical(unsigned int handle, std::string msg, mqtt::async_client *client, unsigned int timeout) : BaseError(handle, msg, client, timeout)
{
    m_priority = BaseError::Priority::CRITICAL;
}

Critical::~Critical()
{
}

void Critical::cancel()
{
    mqtt::message_ptr pubmsg;
    nlohmann::json j;
    
    j["aquarium"]["error"]["type"] = "critical";
    j["aquarium"]["error"]["message"] = "cleared";
    j["aquarium"]["error"]["handle"] = m_handle;
    j["aquarium"]["error"]["timeout"] = m_timeout;

    GpioInterrupt::instance()->setValue(Configuration::instance()->m_redLed, 0);

    pubmsg = mqtt::make_message("aquarium/error", j.dump());
    Configuration::instance()->m_mqtt->publish(pubmsg);
}

void Critical::activate()
{
    mqtt::message_ptr pubmsg;
    nlohmann::json j;
    
    j["aquarium"]["error"]["type"] = "critical";
    j["aquarium"]["error"]["message"] = m_message;
    j["aquarium"]["error"]["handle"] = m_handle;
    j["aquarium"]["error"]["timeout"] = m_timeout;
    
    GpioInterrupt::instance()->setValue(Configuration::instance()->m_redLed, 1);

    pubmsg = mqtt::make_message("aquarium/error", j.dump());
    Configuration::instance()->m_mqtt->publish(pubmsg);
}
