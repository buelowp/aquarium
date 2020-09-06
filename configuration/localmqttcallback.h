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
#include <string>
#include <thread>	// For sleep
#include <atomic>
#include <mqtt/async_client.h>

/**
 * A callback class for use with the main MQTT client.
 */
class callback : public virtual mqtt::callback
{
public:
	void connection_lost(const std::string& cause) override 
	{
		std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Connection lost" << std::endl;
		if (!cause.empty())
			std::cout << "\tcause: " << cause << std::endl;
        if (m_disconnectedCallback) {
            try {
                m_disconnectedCallback(cause);
            }
            catch (std::exception &e) {
                std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Error using callback: " << e.what();
            }
        }
	}

	void delivery_complete(mqtt::delivery_token_ptr tok) override 
	{
	//	std::cout << "\tDelivery complete for token: " << (tok ? tok->get_message_id() : -1) << std::endl;
	}
	
	void connected(const std::string &cause) override 
	{
        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Connected" << std::endl;
        if (m_connectedCallback) {
            try {
                m_connectedCallback();
            }
            catch (std::exception &e) {
                std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Error using callback: " << e.what();
            }
        }
    }
    
    void setConnectedCallback(std::function<void()> cbk) { m_connectedCallback = cbk; }
    void setDisconnectedCallback(std::function<void(const std::string&)> cbk) { m_disconnectedCallback = cbk; }
    
private:
    std::function<void()> m_connectedCallback;
    std::function<void(const std::string&)> m_disconnectedCallback;
};

/////////////////////////////////////////////////////////////////////////////

/**
 * A base action listener.
 */
class action_listener : public virtual mqtt::iaction_listener
{
protected:
	void on_failure(const mqtt::token& tok) override {
		std::cout << "\tListener failure for token: "
			<< tok.get_message_id() << std::endl;
	}

	void on_success(const mqtt::token& tok) override {
		std::cout << "\tListener success for token: "
			<< tok.get_message_id() << std::endl;
	}
};

/////////////////////////////////////////////////////////////////////////////

/**
 * A derived action listener for publish events.
 */
class delivery_action_listener : public action_listener
{
	std::atomic<bool> done_;

	void on_failure(const mqtt::token& tok) override {
		action_listener::on_failure(tok);
		done_ = true;
	}

	void on_success(const mqtt::token& tok) override {
		action_listener::on_success(tok);
		done_ = true;
	}

public:
	delivery_action_listener() : done_(false) {}
	bool is_done() const { return done_; }
};

#endif // MQTTCLIENT_H
