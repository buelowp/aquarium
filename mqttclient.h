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

#ifndef MQTTClient_H
#define MQTTClient_H

#include <string>
#include <functional>
#include <iostream>
#include <cstring>
#include <mosquittopp.h>
#include <cstdio>

class MQTTClient : public mosqpp::mosquittopp
{
public:
    enum CallbackType {
        CONNECT = 0,
        SUBSCRIBE,
        UNSUBSCRIBE,
        PUBLISH,
        DISCONNECT,
    };

    MQTTClient(std::string &id, std::string host, int port = 1883);
    virtual ~MQTTClient();

    bool isConnected() { return m_connected; }
    void setGenericCallback(std::function<void(CallbackType, int)> cbk) { m_genericCallback = cbk; }
    void setMessageCallback(std::function<void(int, std::string, uint32_t*, int)> cbk) { m_messageCallback = cbk; }
    void setErrorCallback(std::function<void(std::string, int)> cbk) { m_errorCallback = cbk; }
    
    void enableDebug(bool debug) { m_debug = debug; }
    
	void on_connect(int rc) override;
    void on_disconnect(int rc) override;
    void on_error() override;
    void on_subscribe(int , int , const int*) override;
    void on_unsubscribe(int) override;
    void on_message(const struct mosquitto_message*) override;
    void on_publish(int) override;
        
private:
    std::string m_name;
    std::string m_host;
    std::function<void(CallbackType, int)> m_genericCallback;
    std::function<void(int, std::string, uint32_t*, int)> m_messageCallback;
    std::function<void(std::string, int)> m_errorCallback;
    int m_port;
    int m_debug;
    int m_connected;
};

#endif // MQTTClient_H
