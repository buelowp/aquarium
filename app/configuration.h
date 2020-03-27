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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <string>
#include <iostream>
#include <memory>
#include <functional>

#include <libconfig.h++>
#include <syslog.h>

#include <mqtt/async_client.h>

#include "potentialhydrogen.h"
#include "dissolvedoxygen.h"
#include "localmqttcallback.h"
#include "flowrate.h"
#include "itimer.h"
#include "temperature.h"
#include "mcp3008.h"

extern void mqttIncomingMessage(std::string topic, std::string message);
extern void mqttConnectionLost(const std::string &cause);
extern void mqttConnected();
extern void aioIncomingMessage(std::string topic, std::string message);
extern void aioConnected();
extern void aioConnectionLost(const std::string &cause);

class Configuration
{
public:
    static const int RED_LED = 15;
    static const int YELLOW_LED = 16;
    static const int GREEN_LED = 17;
    
	static Configuration* instance()
	{
		static Configuration instance;
		return &instance;
	}
    
    bool readConfigFile();
    void setConfigFile(std::string);
    unsigned int nextHandle() { return m_handle++; }
    bool setValue(std::string, std::string);
    bool addArray(std::string, std::map<std::string, std::string>&);
    bool updateArray(std::string, std::map<std::string, std::string>&);
    bool createAIOConnection();
    bool createLocalConnection();
    
    mqtt::async_client *m_aio;
    mqtt::async_client *m_mqtt;
    mqtt::connect_options *m_localConnOpts;
    mqtt::connect_options *m_aioConnOpts;
    mqtt::ssl_options m_localSSLOpts;
    mqtt::ssl_options m_aioSSLOpts;
    LocalMQTTCallback *m_localCallback;
    LocalMQTTCallback *m_aioCallback;
    DissolvedOxygen *m_oxygen;
    PotentialHydrogen *m_ph;
    Temperature *m_temp;
    FlowRate *m_fr;
    MCP3008 *m_adc;
    std::vector<std::string> m_invalidTempDeviceInConfig;
    std::string m_aioServer;
    std::string m_aioUserName;
    std::string m_aioKey;
    std::string m_mqttServer;
    std::string m_mqttUserName;
    std::string m_mqttPassword;
    std::string m_localId;
    std::string m_mcp3008Device;
    std::string m_phVersion;
    std::string m_phVoltage;
    std::string m_phTempComp;
    std::string m_o2Version;
    std::string m_o2Voltage;
    std::string m_o2TempComp;
    bool m_daemonize;
    bool m_aioConnected;
    bool m_mqttConnected;
    bool m_aioEnabled;
    bool m_newTempDeviceFound;
    int m_frEnabled;
    int m_o2SensorAddress;
    int m_phSensorAddress;
    int m_ecSensorAddress;
    int m_onewirepin;
    int m_redLed;
    int m_yellowLed;
    int m_greenLed;
    int m_aioPort;
    int m_flowRatePin;
    int m_mqttPort;
    int m_adcWaterLevelIndex;

private:
    Configuration();
    ~Configuration();
    Configuration& operator=(Configuration const&) {return *this;}
    Configuration(Configuration&);
    
    void generateLocalId();
    bool cisCompare(const std::string & str1, const std::string &str2);
    
    std::string m_configFile;
    unsigned int m_handle;
};

#endif // CONFIGURATION_H
