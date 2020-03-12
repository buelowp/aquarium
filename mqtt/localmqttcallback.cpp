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

#include "localmqttcallback.h"

LocalMQTTCallback::LocalMQTTCallback(mqtt::async_client& cli, mqtt::connect_options& connOpts) : 
    m_retries(0), m_client(cli), m_clientConnOpts(connOpts), m_subListener("Subscription")
{
}

void LocalMQTTCallback::connection_lost(const std::string& cause)
{
    try {
        m_connectionLostCallback();
    }
    catch (std::exception &e) {
        std::cout << __FUNCTION__ << ": Error trying to call the lost connection callback: " << e.what();
    }
}

void LocalMQTTCallback::connected(const std::string& cause)
{
    try {
        m_connectedCallback();
    }
    catch (std::exception &e) {
        std::cout << __FUNCTION__ << ": Error trying to call the connected callback: " << e.what();
    }
}

void LocalMQTTCallback::delivery_complete(mqtt::delivery_token_ptr tok)
{
}

void LocalMQTTCallback::message_arrived(mqtt::const_message_ptr msg)
{
    try {
        m_messageCallback(msg->get_topic(), msg->get_payload_str());
    }
    catch (std::exception &e) {
        std::cout << __FUNCTION__ << ": Error trying to call the lost connection callback: " << e.what();
    }
}

