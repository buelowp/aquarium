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

#include "warning.h"

Warning::Warning()
{
    m_handle = 0;
    m_timeout = 0;
    m_priority = BaseError::Priority::WARNING;
}

Warning::Warning(const Warning& we)
{
    m_priority = we.priority();
    m_message = we.message();
    m_timeout = we.timeout();
    m_handle = we.handle();
    m_mqtt = we.client();
}

Warning::Warning(unsigned int handle, std::string msg, mqtt::async_client *client, unsigned int timeout) : BaseError(handle, msg, client, timeout)
{
    m_priority = BaseError::Priority::WARNING;
}

Warning::~Warning()
{
}

void Warning::cancel()
{
    mqtt::message_ptr pubmsg;
    nlohmann::json j;
    
    j["aquarium"]["error"]["type"] = "warning";
    j["aquarium"]["error"]["message"] = "cleared";
    j["aquarium"]["error"]["handle"] = m_handle;
    j["aquarium"]["error"]["timeout"] = m_timeout;

    digitalWrite(Configuration::instance()->m_yellowLed, 0);

    pubmsg = mqtt::make_message("aquarium/error", j.dump());
    Configuration::instance()->m_mqtt->publish(pubmsg);
}

void Warning::activate()
{
    mqtt::message_ptr pubmsg;
    nlohmann::json j;
    
    j["aquarium"]["error"]["type"] = "warning";
    j["aquarium"]["error"]["message"] = m_message;
    j["aquarium"]["error"]["handle"] = m_handle;
    j["aquarium"]["error"]["timeout"] = m_timeout;

    digitalWrite(Configuration::instance()->m_yellowLed, 1);

    pubmsg = mqtt::make_message("aquarium/error", j.dump());
    Configuration::instance()->m_mqtt->publish(pubmsg);
}
