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

#include "fatal.h"

Fatal::Fatal()
{
    m_handle = 0;
    m_timeout = 0;
    m_priority = BaseError::Priority::FATAL;
}

Fatal::Fatal(const Fatal& fe) 
{
    m_priority = fe.priority();
    m_message = fe.message();
    m_timeout = 0;
    m_handle = fe.handle();
    m_mqtt = fe.client();
}

Fatal::Fatal(unsigned int handle, std::string msg, mqtt::async_client *client , unsigned int timeout) : BaseError(handle, msg, client, 0)
{
    m_priority = BaseError::Priority::FATAL;
}

Fatal::~Fatal()
{
}

/**
 * \fn void Fatal::cancel()
 * 
 * This function just falls through, since we cannot cancel
 * a Fatal, it doesn't do anything.
 */
void Fatal::cancel()
{
}

/**
 * \fn void Fatal::activate()
 * 
 * Will create a fatal error, and the system should stop.
 * Note, this error type cannot be canceled or stopped, so no timer
 * is set in this function.
 */
void Fatal::activate()
{
    mqtt::message_ptr pubmsg;
    nlohmann::json j;
    
    j["aquarium"]["error"]["type"] = "fatal";
    j["aquarium"]["error"]["message"] = m_message;
    j["aquarium"]["error"]["handle"] = m_handle;
    j["aquarium"]["error"]["timeout"] = m_timeout;

    digitalWrite(Configuration::instance()->m_redLed, 1);

    pubmsg = mqtt::make_message("aquarium/error", j.dump());
    Configuration::instance()->m_mqtt->publish(pubmsg);
}
