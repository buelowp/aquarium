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

#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <chrono>
#include <cstring>
#include <functional>
#include <mqtt/async_client.h>

class LocalMQTTCallback : public virtual mqtt::callback, public virtual mqtt::iaction_listener
{
public:
    LocalMQTTCallback(mqtt::async_client& cli, mqtt::connect_options& connOpts);

    void setMessageCallback(std::function<void(std::string, std::string)> cbk) { m_messageCallback = cbk; }
    void setConnectionLostCallback(std::function<void(const std::string&)> cbk) { m_connectionLostCallback = cbk; }
    void setConnectedCallback(std::function<void()> cbk) { m_connectedCallback = cbk; }
    
private:
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr tok) override;
    void connected(const std::string &cause) override;
    
    // TODO: Fix the exit(1) here, it's a little barbaric
	void on_failure(const mqtt::token& tok) override {
		std::cout << "Connection attempt failed" << std::endl;
	}

	// (Re)connection success
	// Either this or connected() can be used for callbacks.
	void on_success(const mqtt::token& tok) override {}

	std::function<void(std::string, std::string)> m_messageCallback;
    std::function<void(const std::string&)> m_connectionLostCallback;
    std::function<void()> m_connectedCallback;
    
	// Counter for the number of connection retries
	int m_retries;
	// The MQTT client
	mqtt::async_client& m_client;
	// Options to use if we need to reconnect
	mqtt::connect_options& m_clientConnOpts;
};

#endif // MQTTCLIENT_H
