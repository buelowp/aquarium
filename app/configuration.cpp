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

#include "configuration.h"

Configuration::Configuration()
{
    m_handle = 1;
}

Configuration::~Configuration()
{

}

void Configuration::setConfigFile(std::string file)
{
    m_configFile = file;
}


bool Configuration::readConfigFile()
{
    config_t config;
    FILE *fs = nullptr;
    const char *aioServer = nullptr;
    const char *aioUserName = nullptr;
    const char *aioKey = nullptr;
    const char *mqttServer = nullptr;
    const char *mqttUserName = nullptr;
    const char *mqttPassword = nullptr;
    const char *localId = nullptr;
    const char *wiredevice = nullptr;
    const char *debug = nullptr;
    const char *spidev = nullptr;
    int aioPort;
    int mqttPort;
    int frpin;
    int owpin;
    int adc;
    int channel;
    int aioEnabled;
    int mqttEnabled;
    int o2sensor;
    int phsensor;
    int red;
    int yellow;
    int green;
    int resolution;
    int wlindex;
    
    std::cout << __FUNCTION__ << ":" << __LINE__ << ": Staring config file read for " << m_configFile << std::endl;
    config_init(&config);
    if (config_read_file(&config, m_configFile.c_str()) == CONFIG_FALSE) {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&config), config_error_line(&config), config_error_text(&config));
        config_destroy(&config);
        return false;
    }

    if (config_lookup_string(&config, "mqtt_name", &localId) == CONFIG_TRUE) {
        if (strlen(localId) > 0)
            m_localId = localId;
        else
            generateLocalId();
    }
    else
        generateLocalId();
    
    syslog(LOG_INFO, "Using %s as our MQTT identifier", m_localId.c_str());
    fprintf(stderr, "Using %s as our MQTT identifier\n", m_localId.c_str());

    if (config_lookup_int(&config, "enable_adafruitio", &aioEnabled) == CONFIG_TRUE) {
        if (aioEnabled == 0)
            m_aioEnabled = false;
        else
            m_aioEnabled = true;
    }
    else
        m_aioEnabled = false;

    if (m_aioEnabled) {
        if (config_lookup_int(&config, "adafruitio_port", &aioPort) == CONFIG_TRUE)
            m_aioPort = aioPort;
        else
            m_aioPort = 8883;
            
        if (config_lookup_string(&config, "adafruitio_server", &aioServer) == CONFIG_TRUE) {
            m_aioServer = aioServer;
            if (m_aioServer.size() == 0) {
                m_aioServer = "io.adafruit.com";
            }
        }
        if (config_lookup_string(&config, "adafruitio_user_name", &aioUserName) == CONFIG_TRUE)
            m_aioUserName = aioUserName;

        if (config_lookup_string(&config, "adafruitio_key", &aioKey) == CONFIG_TRUE)
            m_aioKey = aioKey;

        syslog(LOG_INFO, "Access to AdafruiIO is enabled to %s on port %d", m_aioServer.c_str(), m_aioPort);
        fprintf(stderr, "Access to AdafruiIO is enabled to %s on port %d\n", m_aioServer.c_str(), m_aioPort);
    }
    else  {
        syslog(LOG_INFO, "Access to AdafruitIO is disabled");
        fprintf(stderr, "Access to AdafruitIO is disabled\n");
    }
        
    if (config_lookup_int(&config, "enable_mqtt", &mqttEnabled) == CONFIG_TRUE) {
        if (mqttEnabled == 0)
            m_mqttEnabled = false;
        else
            m_mqttEnabled = true;
    }
    else
        m_mqttEnabled = false;

    if (m_mqttEnabled) {
        if (config_lookup_int(&config, "mqtt_port", &mqttPort) == CONFIG_TRUE)
            m_mqttPort = mqttPort;
        else
            m_mqttPort = 1883;
            
        if (config_lookup_string(&config, "mqtt_server", &mqttServer) == CONFIG_TRUE) {
            m_mqttServer = mqttServer;
            if (m_mqttServer.size() == 0) {
                m_mqttServer = "localhost";
            }
        }
        if (config_lookup_string(&config, "mqtt_user_name", &mqttUserName) == CONFIG_TRUE)
            m_mqttUserName = mqttUserName;
            
        if (config_lookup_string(&config, "mqtt_password", &mqttPassword) == CONFIG_TRUE)
            m_mqttPassword = mqttPassword;
        
        m_mqtt = new MQTTClient(Configuration::instance()->m_localId, Configuration::instance()->m_mqttServer, Configuration::instance()->m_mqttPort);
        syslog(LOG_INFO, "Access to local MQTT is enabled to %s on port %d", m_mqttServer.c_str(), m_mqttPort);
        fprintf(stderr, "Access to local MQTT is enabled to %s on port %d\n", m_mqttServer.c_str(), m_mqttPort);
    }
    else {
        syslog(LOG_INFO, "Access to local MQTT is disabled");
        fprintf(stderr, "Access to local MQTT is disabled\n");
    }
    
    if (config_lookup_int(&config, "onewire_pin", &owpin) == CONFIG_TRUE)
        m_onewirepin = owpin;
    else
        m_onewirepin = 1;
 
    if (config_lookup_int(&config, "red_led", &red) == CONFIG_TRUE)
        m_red_led = red;
    else
        m_red_led = RED_LED;

    if (config_lookup_int(&config, "yellow_led", &yellow) == CONFIG_TRUE)
        m_yellow_led = yellow;
    else
        m_yellow_led = YELLOW_LED;

    if (config_lookup_int(&config, "green_led", &green) == CONFIG_TRUE)
        m_green_led = green;
    else
        m_green_led = GREEN_LED;

    if (config_lookup_int(&config, "waterlevel_index", &wlindex) == CONFIG_TRUE)
        m_adcWaterLevelIndex = wlindex;
    else
        m_adcWaterLevelIndex = 0;
    
    syslog(LOG_INFO, "DS18B20 bus on pin %d", m_onewirepin);
    fprintf(stderr, "DS18B20 bus on pin %d\n", m_onewirepin);
            
    if (config_lookup_int(&config, "flowrate_pin", &frpin) == CONFIG_TRUE)
        m_flowRatePin = frpin;
    else
        m_flowRatePin = 19;
        
    syslog(LOG_INFO, "Monitoring flowrate on pin %d", m_flowRatePin);
    fprintf(stderr, "Monitoring flowrate on pin %d\n", m_flowRatePin);

    if (config_lookup_string(&config, "spi_device", &spidev) == CONFIG_TRUE)
        m_mcp3008Device = spidev;

    if (config_lookup_int(&config, "o2sensor_address", &o2sensor) == CONFIG_TRUE)
        m_o2sensor_address = o2sensor;
    else
        m_o2sensor_address = 0x61;

    if (config_lookup_int(&config, "phsensor_address", &phsensor) == CONFIG_TRUE)
        m_phsensor_address = phsensor;
    else
        m_phsensor_address = 0x63;

    syslog(LOG_INFO, "PH device on i2c address %x", m_phsensor_address);
    fprintf(stderr, "PH device on i2c address %x\n", m_phsensor_address);
    syslog(LOG_INFO, "O2 device on i2c address %x", m_o2sensor_address);
    fprintf(stderr, "O2 device on i2c address %x\n", m_o2sensor_address);
        
    if (config_lookup_string(&config, "debug", &debug) == CONFIG_TRUE) {
        if (cisCompare(debug, "INFO"))
            setlogmask(LOG_UPTO (LOG_INFO));
        if (cisCompare(debug, "ERROR"))
            setlogmask(LOG_UPTO (LOG_ERR));
            
        syslog(LOG_INFO, "Resetting syslog debug level to %s", debug);
        fprintf(stderr, "Resetting syslog debug level to %s\n", debug);
    }

    m_oxygen = new DissolvedOxygen(1, m_o2sensor_address);
    m_ph = new PotentialHydrogen (1, m_phsensor_address);
    m_temp = new Temperature();
    m_adc = new MCP3008(0);
    m_fr = new FlowRate();
    
    config_destroy(&config);
    return true;
}

/**
 * \func void generateLocalId(struct LocalConfig **lc)
 * \param name Pointer to local configuration structure
 * This function attempts to get the kernel hostname for this device and assigns it to name.
 */
void Configuration::generateLocalId()
{
	std::ifstream ifs;
	int pos;

	ifs.open("/proc/sys/kernel/hostname");
	if (!ifs) {
		syslog(LOG_ERR, "Unable to open /proc/sys/kernel/hostname for reading");
		fprintf(stderr, "Unable to open /proc/sys/kernel/hostname for reading");
		m_localId = "Aquarium";
	}
	m_localId.assign((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	try {
		m_localId.erase(m_localId.find('\n'));
	}
	catch (std::out_of_range &e) {
		syslog(LOG_ERR, "handled exception: %s", e.what());
		fprintf(stderr, "handled exception: %s\n", e.what());
	}

    syslog(LOG_INFO, "Assigning %s as device name", m_localId.c_str());
}

bool Configuration::cisCompare(const std::string & str1, const std::string &str2)
{
	return ((str1.size() == str2.size()) && std::equal(str1.begin(), str1.end(), str2.begin(), 
            [](const char c1, const char c2){ return (c1 == c2 || std::toupper(c1) == std::toupper(c2)); }
            ));
}
