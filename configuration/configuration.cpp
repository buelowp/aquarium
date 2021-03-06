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
    m_newTempDeviceFound = false;
}

Configuration::~Configuration()
{
}

void Configuration::setConfigFile(std::string file)
{
    m_configFile = file;
}

bool Configuration::updateArray(std::string array, std::map<std::string, std::string> &entry)
{
    libconfig::Config config;
    std::map<std::string, std::string> newEntries;
    
    try {
        config.readFile(m_configFile.c_str());
    }
    catch(const libconfig::FileIOException &fioex) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << "I/O error while reading file." << std::endl;
        return false;
    }
    catch(const libconfig::ParseException &pex) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << "Parse error at " << pex.getFile() << ":" << pex.getLine() << " - " << pex.getError() << std::endl;
        return false;
    }
    
    libconfig::Setting &root = config.getRoot();
    
    if (!root.exists(array)) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Array " << array.c_str() << "does not exist, use addArray()"  << std::endl;
        return false;
    }

    try {
        libconfig::Setting &arrayEntry = root[array.c_str()];
        for (const auto& [key, value] : entry) {
            std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Searching for " << key << ":" << value << std::endl;
            bool found = false;
            for (int i = 0; i < arrayEntry.getLength(); i++) {
                const libconfig::Setting &device = arrayEntry[i];
                std::string serial;
                std::string name;
                if (!device.lookupValue("device", serial) || !device.lookupValue("name", name)) {
                    std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Unable to find device or name in array..." << std::endl;
                    return false;
                }
                
                if (serial == key) {
                    device["device"] = key;
                    device["name"] = value;
                    found = true;
                }
            }
            if (!found) {
                newEntries[key] = value;
            }
        }
    }
    catch (const libconfig::SettingException &e) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Unable to add elements to the new array: " << e.what() << std::endl;
        return false;
    }
    
    try {
        config.writeFile(m_configFile.c_str());
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Updated configuration successfully written to: " << m_configFile << std::endl;
    }
    catch(const libconfig::FileIOException &fioex) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": I/O error while writing file: " << m_configFile << std::endl;
        return false;
    }
    
    // Must do this last, or we'll read/write the file while another process is reading/writing it
    if (newEntries.size() > 0) {
        addArray(array, newEntries);
    }
    
    return true;
}

bool Configuration::addArray(std::string array, std::map<std::string, std::string> &entry)
{
    libconfig::Config config;
    
    try {
        config.readFile(m_configFile.c_str());
    }
    catch(const libconfig::FileIOException &fioex) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": I/O error while reading file." << std::endl;
        return false;
    }
    catch(const libconfig::ParseException &pex) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Parse error at " << pex.getFile() << ":" << pex.getLine() << " - " << pex.getError() << std::endl;
        return false;
    }
    
    libconfig::Setting &root = config.getRoot();

    if (!root.exists(array)) {
        root.add(array, libconfig::Setting::TypeList);
    }
    
    try {
        libconfig::Setting &arrayEntry = root[array.c_str()];
        for (const auto& [key, value] : entry) {
            libconfig::Setting &device = arrayEntry.add(libconfig::Setting::TypeGroup);
            std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Adding " << key << ":" << value << std::endl;
            device.add("device", libconfig::Setting::TypeString) = key;
            device.add("name", libconfig::Setting::TypeString) = value;
        }
    }
    catch (const libconfig::SettingTypeException &e) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Unable to add elements to the new array: " << e.what() << std::endl;
    }
    
    try {
        config.writeFile(m_configFile.c_str());
        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Updated configuration successfully written to: " << m_configFile << std::endl;
    }
    catch(const libconfig::FileIOException &fioex) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": I/O error while writing file: " << m_configFile << std::endl;
        return false;
    }
    return true;
}

bool Configuration::setValue(std::string key, std::string value)
{
    libconfig::Config config;
    
    try {
        config.readFile(m_configFile.c_str());
    }
    catch(const libconfig::FileIOException &fioex) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": I/O error while reading file." << std::endl;
        return false;
    }
    catch(const libconfig::ParseException &pex) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Parse error at " << pex.getFile() << ":" << pex.getLine() << " - " << pex.getError() << std::endl;
        return false;
    }
    
    libconfig::Setting &root = config.getRoot();
    if (!root.exists(key)) {
        root.add(key, libconfig::Setting::TypeString) = value;
    }
    else {
        libconfig::Setting &entry = root[key.c_str()];
        entry = value;
    }
    
    try {
        config.writeFile(m_configFile.c_str());
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Updated configuration successfully written to: " << m_configFile << std::endl;
    }
    catch(const libconfig::FileIOException &fioex) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": I/O error while writing file: " << m_configFile << std::endl;
        return false;
    }
    return true;
}

bool Configuration::readConfigFile()
{
    libconfig::Config config;
    std::string serial;
    std::string name;
    std::string debug;
    std::map<std::string, std::string> tempDevices;
    bool noDeviceArray = false;
    
    m_temp = new Temperature();
    tempDevices = m_temp->devices();

    std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__  << ": Staring config file read for " << m_configFile << std::endl;
    try {
        config.readFile(m_configFile.c_str());
    }
    catch(const libconfig::FileIOException &fioex) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": I/O error while reading file." << std::endl;
        return false;
    }
    catch(const libconfig::ParseException &pex) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Parse error at " << pex.getFile() << ":" << pex.getLine() << " - " << pex.getError() << std::endl;
        return false;
    }

    libconfig::Setting &root = config.getRoot();
    
    try {
        if (root.exists("mqtt_name")) {
            root.lookupValue("mqtt_name", m_localId);
        }
        else {
            generateLocalId();
        }
        
        syslog(LOG_INFO, "Using %s as our MQTT identifier", m_localId.c_str());
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Using " << m_localId.c_str() << " as our MQTT identifier"  << std::endl;
        
        if (root.exists("enable_adafruitio")) {
            root.lookupValue("enable_adafruitio", m_aioEnabled);
        }
        else {
            m_aioEnabled = false;
        }
        
        if (m_aioEnabled) {
            if (root.exists("adafruitio_port")) {
                root.lookupValue("adafruitio_port", m_aioPort);
            }
            else {
                m_aioPort = 8883;
            }
            if (root.exists("adafruitio_server")) {
                root.lookupValue("adafruitio_server", m_aioServer);
            }
            else {
                m_aioServer = "io.adafruit.com";
            }
            if (root.exists("adafruitio_user_name")) {
                root.lookupValue("adafruitio_user_name", m_aioUserName);
            }
            else {
                m_aioEnabled = false;
                syslog(LOG_ERR, "No AIO username in config, disabling AdafruitIO connection");
                std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": No AIO username in config, disabling AdafruitIO connection" << std::endl;
            }
            if (root.exists("adafruitio_key")) {
                root.lookupValue("adafruitio_key", m_aioKey);
            }
            else {
                m_aioEnabled = false;
                syslog(LOG_ERR, "No AIO key in config, disabling AdafruitIO connection");
                std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": No AIO key in config, disabling AdafruitIO connection" << std::endl;
            }
            
            if (m_aioEnabled) {
                syslog(LOG_INFO, "Access to AdafruiIO is enabled to %s on port %d for user %s", m_aioServer.c_str(), m_aioPort, m_aioUserName.c_str());
                std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Access to AdafruiIO is enabled to " << m_aioServer << " on port " << m_aioPort << " for user " << m_aioUserName.c_str() << ":" << m_aioKey.c_str() << std::endl;
            }
        }
        else {
            syslog(LOG_INFO, "Access to AdafruitIO is disabled");
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Access to AdafruitIO is disabled" << std::endl;
        }
        
        if (root.exists("mqtt_port")) {
            root.lookupValue("mqtt_port", m_mqttPort);
        }
        else {
            m_mqttPort = 1883;
        }
        if (root.exists("mqtt_server")) {
            root.lookupValue("mqtt_server", m_mqttServer);
        }
        else {
            m_mqttServer = "localhost";
        }
        if (root.exists("mqtt_user_name")) {
            root.lookupValue("mqtt_user_name", m_mqttUserName);
            if (root.exists("mqtt_password")) {
                root.lookupValue("mqtt_password", m_mqttPassword);
            }
            syslog(LOG_INFO, "MQTT is connecting to %s:%d for user %s", m_mqttServer.c_str(), m_mqttPort, m_mqttUserName.c_str());
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": MQTT is connecting to " << m_mqttServer << ":" << m_mqttPort << " for user " << m_mqttUserName.c_str() << std::endl;
        }
        else {
            syslog(LOG_INFO, "MQTT is connecting to %s:%d", m_mqttServer.c_str(), m_mqttPort);
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": MQTT is connecting to " << m_mqttServer.c_str() << ":" << m_mqttPort << std::endl;
        }
        
        if (root.exists("onewire_pin")) {
            root.lookupValue("onewire_pin", m_onewirepin);
            syslog(LOG_INFO, "DS18B20 bus on pin %d", m_onewirepin);
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": DS18B20 bus on pin " << m_onewirepin << std::endl;
        }
        else {
            m_onewirepin = -1;
        }
        
        // If not found, these default to the RPi setup
        if (root.exists("red_led")) {
            root.lookupValue("red_led", m_redLed);
        }
        else {
            m_redLed = 23;
        }
        if (root.exists("yellow_led")) {
            root.lookupValue("yellow_led", m_yellowLed);
        }
        else {
            m_yellowLed = 24;
        }
        if (root.exists("green_led")) {
            root.lookupValue("green_led", m_greenLed);
        }
        else {
            m_greenLed = 25;
        }
        
        if (root.exists("waterlevel_index")) {
            root.lookupValue("waterlevel_index", m_adcWaterLevelIndex);
        }
        else {
            m_adcWaterLevelIndex = 0;
        }
        
        if (root.exists("gpio_one")) {
            root.lookupValue("gpio_one", m_gpioPortOne);
            syslog(LOG_INFO, "GPIO Port One toggle set to pin %d", m_gpioPortOne);
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": GPIO Toggle set to pin " << m_gpioPortOne << std::endl;
        }
        else {
            m_gpioPortOne = 0;
            syslog(LOG_INFO, "GPIO Port One disabled");
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": GPIO Port One disabled" << std::endl;
        }

        if (root.exists("gpio_two")) {
            root.lookupValue("gpio_one", m_gpioPortTwo);
            syslog(LOG_INFO, "GPIO Port Two toggle set to pin %d", m_gpioPortTwo);
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": GPIO Port Two Toggle set to pin " << m_gpioPortTwo << std::endl;
        }
        else {
            m_gpioPortTwo = 0;
            syslog(LOG_INFO, "GPIO Port Two disabled");
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": GPIO Port Two disabled" << std::endl;
        }

        if (root.exists("phsensor_address")) {
            root.lookupValue("phsensor_address", m_phSensorAddress);
            syslog(LOG_INFO, "PH device on i2c address %x", m_phSensorAddress);
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": PH device on i2c address " << m_phSensorAddress << std::endl;
        }
        else {
            m_phSensorAddress = 0;
            syslog(LOG_INFO, "PH device disabled");
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": PH device disabled" << std::endl;
        }

        if (root.exists("o2sensor_address")) {
            root.lookupValue("o2sensor_address", m_o2SensorAddress);
            syslog(LOG_INFO, "Oxygen sensor on i2c address %x", m_o2SensorAddress);
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Oxygen sensor on i2c address " << m_o2SensorAddress << std::endl;
        }
        else {
            m_o2SensorAddress = 0;
            syslog(LOG_INFO, "Oxygen sensor disabled");
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Oxygen sensor disabled" << std::endl;
        }
        
        if (root.exists("ecsensor_address")) {
            root.lookupValue("ecsensor_address", m_ecSensorAddress);
            syslog(LOG_INFO, "Conductivity sensor on i2c address %x", m_ecSensorAddress);
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Conductivity sensor on i2c address " << m_ecSensorAddress << std::endl;
        }
        else {
            m_ecSensorAddress = 0;
            syslog(LOG_INFO, "Conductivity sensor disabled");
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Conductivity sensor disabled" << std::endl;
        }

        try {
            if (root.exists("debug")) {
                root.lookupValue("debug", debug);
                if (cisCompare(debug, "INFO"))
                    setlogmask(LOG_UPTO (LOG_INFO));
                else if (cisCompare(debug, "WARNING"))
                    setlogmask(LOG_UPTO (LOG_WARNING));
                else if (cisCompare(debug, "ERROR"))
                    setlogmask(LOG_UPTO (LOG_ERR));
            }
            else {
                setlogmask(LOG_UPTO (LOG_WARNING));
            }
        }
        catch (libconfig::SettingException &e) {
            syslog(LOG_ERR, "Error configuring logging: : %s", e.what());
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Error configuring logging: " << e.what() << std::endl;
        }
        
        try {
            if (root.exists("ds18b20")) {
                const libconfig::Setting &probe = root["ds18b20"];
                if (tempDevices.size() > probe.getLength()) {
                    syslog(LOG_WARNING, "New DS18B20 device detected, adding to configuration");
                    std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": New DS18B20 device detected, adding to configuration" << std::endl;                    
                    m_newTempDeviceFound = true;
                }
                for (int i = 0; i < probe.getLength(); i++) {
                    const libconfig::Setting &device = probe[i];
                    device.lookupValue("device", serial);
                    device.lookupValue("name", name);
                    
                    auto found = tempDevices.find(serial);
                    if (found != tempDevices.end()) {
                        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Renaming DS18B20 device " << serial << " to " << name << std::endl;
                        m_temp->setNameForDevice(serial, name);
                    }
                    else { // TODO: Figure out how to report this as an error!
                        m_invalidTempDeviceInConfig.push_back(serial);
                        syslog(LOG_WARNING, "DS18B20 probe %s in config, but not connected...", serial.c_str());
                        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": DS18B20 probe " << serial << " in config, but not connected..." << std::endl;
                    }
                }
            }
            else {
                if (tempDevices.size())
                    noDeviceArray = true;
            }
        }
        catch (libconfig::SettingException &e) {
            syslog(LOG_ERR, "Cannot update ds18b20 array: %s", e.what());
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Cannot update ds18b20 array: " << e.what() << std::endl;
        }
            
    }
    catch (libconfig::SettingException &e) {
        syslog(LOG_ERR, "SettingException: %s", e.what());
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": SettingException: " << e.what() << std::endl;
    }

    if (noDeviceArray)
        addArray("ds18b20", tempDevices);
    
    if (m_newTempDeviceFound)
        updateArray("ds18b20", tempDevices);
    
    m_oxygen = new DissolvedOxygen(1, m_o2SensorAddress);
    m_ph = new PotentialHydrogen (1, m_phSensorAddress);
    m_adc = new MCP3008(0);
    
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

	ifs.open("/proc/sys/kernel/hostname");
	if (!ifs) {
		syslog(LOG_ERR, "Unable to open /proc/sys/kernel/hostname for reading");
		std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Unable to open /proc/sys/kernel/hostname for reading" << std::endl;
		m_localId = "Aquarium";
	}
	m_localId.assign((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	try {
		m_localId.erase(m_localId.find('\n'));
	}
	catch (std::out_of_range &e) {
		syslog(LOG_ERR, "handled exception: %s", e.what());
		std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": handled exception: " << e.what() << std::endl;
	}

    syslog(LOG_INFO, "Assigning %s as device name", m_localId.c_str());
}

bool Configuration::cisCompare(const std::string & str1, const std::string &str2)
{
	return ((str1.size() == str2.size()) && std::equal(str1.begin(), str1.end(), str2.begin(), 
            [](const char c1, const char c2){ return (c1 == c2 || std::toupper(c1) == std::toupper(c2)); }
            ));
}

bool Configuration::createLocalConnection()
{
    std::string server("tcp://");

    mqtt::connect_options connopts;
    connopts.set_keep_alive_interval(20);
    connopts.set_clean_session(true);
    connopts.set_automatic_reconnect(1, 10);

    server += m_mqttServer;
    server += ":";
    server += std::to_string(m_mqttPort);
    std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": " << server << std::endl;

    m_mqtt = new mqtt::async_client(server, m_localId);
    m_localCallback.setConnectedCallback(mqttConnected);
    m_localCallback.setDisconnectedCallback(mqttConnectionLost);
    m_mqtt->set_callback(m_localCallback);

    try {
        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Connecting to the Local MQTT server at: " << m_mqtt->get_server_uri() << std::endl << std::flush;
        m_mqtt->connect(connopts);
    }
    catch (const mqtt::exception&) {
        std::cerr << __PRETTY_FUNCTION__ << ": ERROR: Unable to connect to MQTT server: '" << m_mqtt->get_server_uri() << "'" << std::endl;
        return false;
    }
    return true;
}

bool Configuration::createAIOConnection()
{
    if (!m_aioEnabled)
        return false;

    std::string server("tcp://");

    if (m_aioPort == 8883) {
        m_aioConnOpts = new mqtt::connect_options();
        m_aioSSLOpts.set_private_key(m_aioUserName);
        m_aioSSLOpts.set_private_key_password(m_aioKey);
        m_aioConnOpts->set_ssl(m_aioSSLOpts);
    }
    else {
        m_aioConnOpts = new mqtt::connect_options(m_aioUserName, m_aioKey);
    }

    m_aioConnOpts->set_keep_alive_interval(20);
    m_aioConnOpts->set_clean_session(true);
    m_aioConnOpts->set_automatic_reconnect(1, 10);

    server += m_aioServer;
    server += ":";
    server += std::to_string(m_aioPort);

    m_aio = new mqtt::async_client(server, m_localId);
    m_aioCallback.setConnectedCallback(aioConnected);
    m_aioCallback.setDisconnectedCallback(aioConnectionLost);
    m_aio->set_callback(m_aioCallback);

    try {
        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << "Connecting to AIO server at " << server << std::endl << std::flush;
        m_aio->connect(*m_aioConnOpts);
    }
    catch (const mqtt::exception&) {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << "ERROR: Unable to connect to AIO server: '" << m_aioServer << "'" << std::endl;
        return false;
    }
    return true;
}

