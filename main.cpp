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

#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include <gpiopp.h>
#include <adaio.h>
#include <libconfig.h>

#include "potentialhydrogen.h"
#include "dissolvedoxygen.h"
#include "mqttclient.h"
#include "flowrate.h"

#define DEFAULT_FR_PIN      15

AdafruitIO *g_aio;
MQTTClient *g_mqtt;
bool g_aioConnected;
bool g_mqttConnected;

struct localConfig {
    std::string aioServer;
    int aioPort;
    std::string aioUserName;
    std::string aioKey;
    std::string mqttServer;
    int mqttPort;
    std::string mqttUserName;
    std::string mqttPassword;
    std::string localId;
    int flowRatePin;
    std::string configFile;
    bool debug;
};

void setConfigDefaults(struct localConfig *lc)
{
    lc->aioPort = 8883;
    lc->mqttPort = 1883;
    lc->aioServer = "io.adafruit.com";
    lc->flowRatePin = 15;
    lc->configFile = "~/.config/aquarium.conf";
    lc->debug = false;
}


/**
 * \func void generateLocalId(struct localConfig **lc)
 * \param name Pointer to local configuration structure
 * This function attempts to get the kernel hostname for this device and assigns it to name.
 */
void generateLocalId(struct localConfig **lc)
{
    struct localConfig *llc = *lc;
	std::ifstream ifs;
	int pos;

	ifs.open("/proc/sys/kernel/hostname");
	if (!ifs) {
		std::cerr << __PRETTY_FUNCTION__ << ": Unable to open /proc/sys/kernel/hostname for reading" << std::endl;
		llc->localId = "omega";
	}
	llc->localId.assign((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	try {
		llc->localId.erase(llc->localId.find('\n'));
	}
	catch (std::out_of_range &e) {
		std::cerr << __PRETTY_FUNCTION__ << ": " << e.what() << std::endl;
	}
	if (llc->debug)
        std::cout << __PRETTY_FUNCTION__ << ": Assigning " << llc->localId << " as device name" << std::endl;
}

bool readConfig(struct localConfig *lc)
{
    config_t *config;
    FILE *fs = nullptr;
    const char *aioServer = nullptr;
    const char *aioUserName = nullptr;
    const char *aioKey = nullptr;
    const char *mqttServer = nullptr;
    const char *mqttUserName = nullptr;
    const char *mqttPassword = nullptr;
    const char *localId = nullptr;
    int aioPort;
    int mqttPort;
    int frpin;
    
    config_init(config);
    fs = fopen(lc->configFile.c_str(), "r");
    if (fs) {
        if (config_read(config, fs) == CONFIG_FALSE) {
            onionPrint(ONION_SEVERITY_FATAL, "config_read: error in file %s, line %d\n", config_error_file(config), config_error_line(config));
            return false;
        }
        if (config_lookup_int(config, "local_mqtt_port", &mqttPort) == CONFIG_TRUE)
            lc->mqttPort = mqttPort;
        
        if (config_lookup_int(config, "adafruitio_port", &aioPort) == CONFIG_TRUE)
            lc->aioPort = aioPort;
        
        if (config_lookup_string(config, "adafruitio_server", &aioServer) == CONFIG_TRUE)
            lc->aioServer = aioServer;
        
        if (config_lookup_string(config, "adafruitio_user_name", &aioUserName) == CONFIG_TRUE)
            lc->aioUserName = aioUserName;
        
        if (config_lookup_string(config, "adafruitio_key", &aioKey) == CONFIG_TRUE)
            lc->aioKey = aioKey;
        
        if (config_lookup_string(config, "mqtt_server", &mqttServer) == CONFIG_TRUE)
            lc->mqttServer = mqttServer;
        
        if (config_lookup_string(config, "mqtt_user_name", &mqttUserName) == CONFIG_TRUE)
            lc->mqttUserName = mqttUserName;
        
        if (config_lookup_string(config, "mqtt_password", &mqttPassword) == CONFIG_TRUE)
            lc->mqttPassword = mqttPassword;
        
        if (config_lookup_string(config, "mqtt_name", &localId) == CONFIG_TRUE)
            lc->localId = localId;
        else
            generateLocalId(&lc);
        
        if (config_lookup_int(config, "flowrate_pin", &frpin) == CONFIG_TRUE)
            lc->flowRatePin = frpin;
    }
    
    config_destroy(config);
}

void flowRateCallback(GpioMetaData *md)
{
}

void aioGenericCallback(AdafruitIO::CallbackType type, int mid)
{
    switch (type) {
        case AdafruitIO::CallbackType::CONNECT:
            g_aioConnected = true;
            break;
        case AdafruitIO::CallbackType::DISCONNECT:
            g_aioConnected = false;
            break;
        defaut:
            break;
    }
}

void mqttGenericCallback(MQTTClient::CallbackType type, int mid)
{
    switch (type) {
        case MQTTClient::CallbackType::CONNECT:
            g_mqttConnected = true;
            break;
        case MQTTClient::CallbackType::DISCONNECT:
            g_mqttConnected = false;
            break;
        defaut:
            break;
    }
}

// ttps://io.adafruit.com/api/v2/{username}/feeds/{feed_key}/data
void sendAioData(std::string &topic, uint8_t *payload, int payloadlen)
{
    g_aio->publish(NULL, topic.c_str(), payloadlen, payload);
}

void sendLocalData(std::string &topic, uint8_t *payload, int payloadlen)
{
    g_mqtt->publish(NULL, topic.c_str(), payloadlen, payload);
}

void usage(const char *name)
{
    std::cerr << "usage: " << name << " -h <server> -p <port> -n <unique id> -u <username> -k <password/key> -d" << std::endl;
    std::cerr << "\t-c alternate configuration file (defaults to $HOME/.config/aquarium.conf" << std::endl;
    std::cerr << "\t-h Print usage and exit" << std::endl;
    std::cerr << "\t-d Daemonize the application to run in the background" << std::endl;
    exit(-1);
}

/**
 * \func bool parse_args(int argc, char **argv, std::string &name, std::string &mqtt, int &port)
 * \param arc integer argument count provied by shell
 * \param argv char ** array of arguments provided by shell
 * \param name std::string name of device which would be overridden with -n
 * \param mqtt std::string hostname or IP of Mosquitto server to connect to
 * \param username std::string username for adafruit IO
 * \param apikey std::string api key for adafruit IO
 * \param port integer port number to override default Mosquitto server port number
 * Use getopt to set runtime arguments used by this server
 */
bool parse_args(int argc, char **argv, std::string &config, bool &daemonize)
{
	int opt;
	bool rval = true;
    std::vector<int> args;
    
	if (argv) {
		while ((opt = getopt(argc, argv, "c:hd")) != -1) {
			switch (opt) {
            case 'h':
                usage(argv[0]);
                break;
			case 'c':
				config = optarg;
				break;
	        case 'd':
	            daemonize = true;
	            break;
	        default:
	            usage(argv[0]);
			}
		}
	}

	return rval;
}

void mainloop()
{
    unsigned long count = 0;
    
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        g_aio->loop();
        g_mqtt->loop();
    }
}

int main(int argc, char *argv[])
{
    DissolvedOxygen oxygen(0, 0x61);
    PotentialHydrogen ph(0, 0x62);
    FlowRate fr;
    struct localConfig lc;
    bool daemonize = false;
    
    if (argc < 3)
        usage(argv[0]);
    
    onionSetVerbosity(ONION_VERBOSITY_VERBOSE);
    
    setConfigDefaults(&lc);
    
    parse_args(argc, argv, lc.configFile, daemonize);

    g_aio = new AdafruitIO(lc.localId, lc.aioServer, lc.aioUserName, lc.aioKey, lc.aioPort);
    g_mqtt = new MQTTClient(lc.localId, lc.mqttServer, lc.mqttPort);
    
    GpioMetaData flow(lc.flowRatePin);
    flow.setInterruptType(GpioMetaData::GPIO_Irq_Type::GPIO_IRQ_RISING);
    flow.setCallback(flowRateCallback);
    GpioInterrupt::instance()->set(&flow);
    GpioInterrupt::instance()->start();

    mainloop();
    
    return 0;
}
