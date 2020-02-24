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

#include <libconfig.h>
#include <syslog.h>

#include <adaio.h>
#include "potentialhydrogen.h"
#include "dissolvedoxygen.h"
#include "mqttclient.h"
#include "flowrate.h"
#include "itimer.h"
#include "ds18b20.h"
#include "mcp3008.h"
#include "errorhandler.h"

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
    
    AdafruitIO *m_aio;
    MQTTClient *m_mqtt;
    DissolvedOxygen *m_oxygen;
    PotentialHydrogen *m_ph;
    DS18B20 *m_temp;
    FlowRate *m_fr;
    MCP3008 *m_adc;
    ErrorHandler m_errors;
    std::string m_aioServer;
    std::string m_aioUserName;
    std::string m_aioKey;
    std::string m_mqttServer;
    std::string m_mqttUserName;
    std::string m_mqttPassword;
    std::string m_localId;
    std::string m_mcp3008Device;
    bool m_daemonize;
    bool m_aioConnected;
    bool m_mqttConnected;
    bool m_aioEnabled;
    bool m_mqttEnabled;
    int m_o2sensor_address;
    int m_phsensor_address;
    int m_onewirepin;
    int m_red_led;
    int m_yellow_led;
    int m_green_led;
    int m_aioPort;
    int m_flowRatePin;
    int m_mqttPort;

private:
    Configuration();
    ~Configuration();
    Configuration& operator=(Configuration const&) {};
    Configuration(Configuration&);
    
    void generateLocalId();
    bool cisCompare(const std::string & str1, const std::string &str2);
    
    std::string m_configFile;
    unsigned int m_handle;
};

#endif // CONFIGURATION_H
