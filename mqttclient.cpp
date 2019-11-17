/*
 * Copyright (c) 2019 Peter Buelow <goballstate at gmail dot com>
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

#include "mqttclient.h"

/**
 * \func MQTTClient::MQTTClient(std::string &id, std::string &host, std::string &username, std::string &password, int port)
 * \param id MQTT connection name
 * \param host MQTT hostname to connect to
 * \param username MQTT connection username
 * \param password MQTT connection password
 * \param port MQTT port to connect to
 * 
 * This will create a new connection and start the loop. It does not return an error, that would be handled by callbacks. Note
 * that this implementation will resuse username and password for both the standard connection and TLS options. This is specific
 * to MQTTClient and may not be compatible with other cloud providers.
 */
MQTTClient::MQTTClient(std::string &id, std::string host, int port) : mosqpp::mosquittopp(id.c_str()),
    m_name(id), m_host(host), m_port(port)
{
    m_debug = false;
    m_connected = false;
    mosqpp::lib_init();			// Initialize libmosquitto

	int keepalive = 120; // seconds
    
	connect_async(m_host.c_str(), m_port, keepalive);
    loop_start();
}

/**
 * \func MQTTClient::~MQTTClient()
 * 
 * Cleanup the MQTT connecytion before destroying the object
 */
MQTTClient::~MQTTClient()
{
    loop_stop();            // Kill the thread
    mosqpp::lib_cleanup();    // Mosquitto library cleanup
}

/**
 * \func void MQTTClient::on_connect(int rc)
 * \param rc The callback code indicating success or failure and failure reason
 * 
 * Called when the unit connects. It will attempt to fire a callback to the calling
 * program if one is set. The callback will include the reason code.
 */
void MQTTClient::on_connect(int rc)
{
    if (rc != 0) {
        std::cerr << __FUNCTION__ << ": Unable to connect with rc " << rc << std::endl;
        m_connected = false;
        return;
    }
    
    if (m_debug)
        std::cerr << __FUNCTION__ << "Connected with code " << rc << std::endl;
    
    if (m_genericCallback) {
        try {
            m_genericCallback(CallbackType::CONNECT, rc);
        }
        catch (std::bad_function_call e) {
            std::cerr << __FUNCTION__ << "Caught exception " << e.what() << std::endl;
            return;
        }
    }
}

void MQTTClient::on_disconnect(int rc)
{
    if (m_debug)
        std::cerr << __FUNCTION__ << ": Disconnected with code " << rc << std::endl;

    if (m_genericCallback) {
        try {
            m_genericCallback(CallbackType::DISCONNECT, rc);
        }
        catch (std::bad_function_call e) {
            std::cerr << __FUNCTION__ << "Caught exception " << e.what() << std::endl;
            return;
        }
    }
    m_connected = false;
}

void MQTTClient::on_subscribe(int mid, int, const int*)
{
    if (m_genericCallback) {
        try {
            m_genericCallback(CallbackType::SUBSCRIBE, mid);
        }
        catch (std::bad_function_call e) {
            std::cerr << __FUNCTION__ << "Caught exception " << e.what() << std::endl;
            return;
        }
    }
}

void MQTTClient::on_error()
{
    if (m_errorCallback) {
        try {
            m_errorCallback(std::string("UNKNOWN"), errno);
        }
        catch (std::bad_function_call e) {
            std::cerr << __FUNCTION__ << "Caught exception " << e.what() << std::endl;
            return;
        }
    }
}

void MQTTClient::on_message(const struct mosquitto_message *msg)
{
    std::string topic = msg->topic;
    uint32_t payload[2048];
    
    if (m_messageCallback) {
        memset(payload, 0, 2048);
        memcpy(payload, msg->payload, msg->payloadlen);
        
        try {
            m_messageCallback(msg->mid, topic, payload, msg->payloadlen);
        }
        catch (std::bad_function_call e) {
            std::cerr << __FUNCTION__ << "Caught exception " << e.what() << std::endl;
            return;
        }
    }
}

void MQTTClient::on_publish(int mid)
{
    if (m_genericCallback) {
        try {
            m_genericCallback(CallbackType::PUBLISH, mid);
        }
        catch (std::bad_function_call e) {
            std::cerr << __FUNCTION__ << "Caught exception " << e.what() << std::endl;
            return;
        }
    }
}

void MQTTClient::on_unsubscribe(int mid)
{
    if (m_genericCallback) {
        try {
            m_genericCallback(CallbackType::UNSUBSCRIBE, mid);
        }
        catch (std::bad_function_call e) {
            std::cerr << __FUNCTION__ << "Caught exception " << e.what() << std::endl;
            return;
        }
    }
}
