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
#include <gpiointerrupt.h>
#include <adaio.h>
#include <libconfig.h>
#include <syslog.h>
#include <libgen.h>

#include "potentialhydrogen.h"
#include "dissolvedoxygen.h"
#include "mqttclient.h"
#include "flowrate.h"
#include "itimer.h"

#define DEFAULT_FR_PIN      15
#define ONE_SECOND          1000
#define ONE_MINUTE          (ONE_SECOND * 60)
#define FIFTEEN_MINUTES     (ONE_MINUTE * 15)

AdafruitIO *g_aio;
MQTTClient *g_mqtt;
bool g_aioConnected;
bool g_mqttConnected;

struct LocalConfig {
    AdafruitIO *g_aio;
    MQTTClient *g_mqtt;
    DissolvedOxygen *oxygen;
    PotentialHydrogen *ph;
    FlowRate *fr;
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
    bool daemonize;
    bool aioConnected;
    bool mqttConnected;
};

void phCallback(int cmd, std::string response)
{
    if (cmd == PotentialHydrogen::INFO) {
        syslog(LOG_NOTICE, "got PH probe info string %s\n", response.c_str());
    }
}

void doCallback(int cmd, std::string response)
{
    if (cmd == DissolvedOxygen::INFO) {
        syslog(LOG_NOTICE, "got DO probe info string %s\n", response.c_str());
    }
}

void setConfigDefaults(struct LocalConfig &lc)
{
    lc.aioPort = 8883;
    lc.mqttPort = 1883;
    lc.aioServer = "io.adafruit.com";
    lc.flowRatePin = 15;
    lc.configFile = "~/.config/aquarium.conf";
    lc.debug = false;
    lc.daemonize = false;
    lc.aioConnected = false;
    lc.mqttConnected = false;
    lc.oxygen = new DissolvedOxygen(0, 0x61);
    lc.ph = new PotentialHydrogen (0, 0x63);
    lc.fr = new FlowRate();
    
    lc.oxygen->setCallback(doCallback);
    lc.ph->setCallback(phCallback);
}


/**
 * \func void generateLocalId(struct LocalConfig **lc)
 * \param name Pointer to local configuration structure
 * This function attempts to get the kernel hostname for this device and assigns it to name.
 */
void generateLocalId(struct LocalConfig **lc)
{
    struct LocalConfig *llc = *lc;
	std::ifstream ifs;
	int pos;

	ifs.open("/proc/sys/kernel/hostname");
	if (!ifs) {
		syslog(LOG_ERR, "Unable to open /proc/sys/kernel/hostname for reading");
		llc->localId = "omega";
	}
	llc->localId.assign((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	try {
		llc->localId.erase(llc->localId.find('\n'));
	}
	catch (std::out_of_range &e) {
		syslog(LOG_ERR, "handled exception: %s", e.what());
	}
	if (llc->debug)
        syslog(LOG_NOTICE, "Assigning %s as device name", llc->localId.c_str());
}

bool readConfig(struct LocalConfig *lc)
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
            syslog(LOG_ERR, "config_read: error in file %s, line %d\n", config_error_file(config), config_error_line(config));
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

void flowRateCallback(GpioInterrupt::MetaData *md)
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
bool parse_args(int argc, char **argv, struct LocalConfig &config)
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
				config.configFile = optarg;
				break;
	        case 'd':
	            config.daemonize = true;
	            break;
	        default:
                syslog(LOG_ERR, "Unexpected command line argument given");
	            usage(argv[0]);
			}
		}
	}

	return rval;
}

void mainloop(struct LocalConfig &lc)
{
    ITimer doUpdate;
    ITimer phUpdate;
    auto phfunc = [lc]() { lc.ph->sendStatusCommand(); };
    auto dofunc = [lc]() { lc.oxygen->sendStatusCommand(); };
    
    doUpdate.setInterval(dofunc, ONE_MINUTE);
    phUpdate.setInterval(phfunc, ONE_MINUTE);
    
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (lc.aioConnected)
            lc.g_aio->loop();
        
        if (lc.mqttConnected)
            lc.g_mqtt->loop();
    }
    
    doUpdate.stop();
    phUpdate.stop();
}

int main(int argc, char *argv[])
{
    struct LocalConfig lc;
    std::string progname = basename(argv[0]);
    
    setlogmask (LOG_UPTO (LOG_INFO));
    openlog(progname.c_str(), LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    
    syslog(LOG_NOTICE, "Application starting");
    setConfigDefaults(lc);
    
    parse_args(argc, argv, lc);

    g_aio = new AdafruitIO(lc.localId, lc.aioServer, lc.aioUserName, lc.aioKey, lc.aioPort);
    g_mqtt = new MQTTClient(lc.localId, lc.mqttServer, lc.mqttPort);
    
    GpioInterrupt::instance()->addPin(lc.flowRatePin);
    GpioInterrupt::instance()->setPinCallback(lc.flowRatePin, flowRateCallback);
    GpioInterrupt::instance()->start(lc.flowRatePin);

    lc.oxygen->sendInfoCommand();
    lc.ph->sendInfoCommand();
    
    mainloop(lc);
    
    return 0;
}
