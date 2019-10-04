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
#include <cstring>

#include <curl/curl.h>
#include <gpiointerrupt.h>
#include <adaio.h>
#include <libconfig.h>
#include <syslog.h>
#include <libgen.h>
#include <errno.h>
#include <mcp3424.h>

#include "potentialhydrogen.h"
#include "dissolvedoxygen.h"
#include "mqttclient.h"
#include "flowrate.h"
#include "itimer.h"
#include "temperature.h"

#define DEFAULT_FR_PIN      15
#define ONE_SECOND          1000
#define ONE_MINUTE          (ONE_SECOND * 60)
#define FIFTEEN_MINUTES     (ONE_MINUTE * 15)

#define AIO_FLOWRATE_FEED   "pbuelow/feeds/aquarium.flowrate"
#define AIO_OXYGEN_FEED     "pbuelow/feeds/aquarium.oxygen"
#define AIO_PH_FEED         "pbuelow/feeds/aquarium.ph"
#define AIO_TEMP_FEED       "pbuelow/feeds/aquarium.Temperature"
#define AIO_LEVEL_FEED      "pbuelow/feeds/aquarium.waterlevel"

#define RAW_MIN 0
#define RAW_MAX_12 2048
#define RAW_MAX_14 8192
#define RAW_MAX_16 32768
#define RAW_MAX_18 131072
#define V_MIN 0.00
#define V_MAX 3.30

AdafruitIO *g_aio;
MQTTClient *g_mqtt;
bool g_aioConnected;
bool g_mqttConnected;

struct LocalConfig {
    AdafruitIO *g_aio;
    MQTTClient *g_mqtt;
    DissolvedOxygen *oxygen;
    PotentialHydrogen *ph;
    Temperature *temp;
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
    std::string tempDevice;
    bool daemonize;
    bool aioConnected;
    bool mqttConnected;
    bool aioEnabled;
    bool mqttEnabled;
    mcp3424 *adc;
    mcp3424_channel adc_channel;
    int adc_address;
    int o2sensor_address;
    int phsensor_address;
    int onewirepin;
};

bool cisCompare(const std::string & str1, const std::string &str2)
{
	return ((str1.size() == str2.size()) && std::equal(str1.begin(), str1.end(), str2.begin(), 
            [](const char c1, const char c2){ return (c1 == c2 || std::toupper(c1) == std::toupper(c2)); }
            ));
}

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
    lc.configFile = "~/.config/aquarium.conf";
    lc.daemonize = false;
    lc.fr = new FlowRate();
    lc.adc = new mcp3424();
}


/**
 * \func void generateLocalId(struct LocalConfig **lc)
 * \param name Pointer to local configuration structure
 * This function attempts to get the kernel hostname for this device and assigns it to name.
 */
void generateLocalId(struct LocalConfig *lc)
{
	std::ifstream ifs;
	int pos;

	ifs.open("/proc/sys/kernel/hostname");
	if (!ifs) {
		syslog(LOG_ERR, "Unable to open /proc/sys/kernel/hostname for reading");
		fprintf(stderr, "Unable to open /proc/sys/kernel/hostname for reading");
		lc->localId = "omega";
	}
	lc->localId.assign((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	try {
		lc->localId.erase(lc->localId.find('\n'));
	}
	catch (std::out_of_range &e) {
		syslog(LOG_ERR, "handled exception: %s", e.what());
		fprintf(stderr, "handled exception: %s\n", e.what());
	}

    syslog(LOG_INFO, "Assigning %s as device name", lc->localId.c_str());
    fprintf(stderr, "Assigning %s as device name\n", lc->localId.c_str());
}

bool readConfig(struct LocalConfig *lc)
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
    
    config_init(&config);
    if (config_read_file(&config, lc->configFile.c_str()) == CONFIG_FALSE) {
//        syslog(LOG_ERR, "config_read: error in file %s, line %d\n", config_error_file(&config), config_error_line(&config));
//        std::cerr << __FUNCTION__ << ": config_read: error in file " << config_error_file(&config) << ", line " << config_error_line(&config) << std::endl;
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&config), config_error_line(&config), config_error_text(&config));
        config_destroy(&config);
        return false;
    }
    else {
        if (config_lookup_string(&config, "mqtt_name", &localId) == CONFIG_TRUE) {
            if (strlen(localId) > 0)
                lc->localId = localId;
            else
                generateLocalId(lc);
        }
        else
            generateLocalId(lc);

        syslog(LOG_INFO, "Using %s as our MQTT identifier", lc->localId.c_str());
        fprintf(stderr, "Using %s as our MQTT identifier\n", lc->localId.c_str());

        if (config_lookup_int(&config, "enable_adafruitio", &aioEnabled) == CONFIG_TRUE) {
            if (aioEnabled == 0)
                lc->aioEnabled = false;
            else
                lc->aioEnabled = true;
        }
        else
            lc->aioEnabled = false;

        if (lc->aioEnabled) {
            if (config_lookup_int(&config, "adafruitio_port", &aioPort) == CONFIG_TRUE)
                lc->aioPort = aioPort;
            else
                lc->aioPort = 8883;
            
            if (config_lookup_string(&config, "adafruitio_server", &aioServer) == CONFIG_TRUE) {
                lc->aioServer = aioServer;
                if (lc->aioServer.size() == 0) {
                    lc->aioServer = "io.adafruit.com";
                }
            }
            if (config_lookup_string(&config, "adafruitio_user_name", &aioUserName) == CONFIG_TRUE)
                lc->aioUserName = aioUserName;

            if (config_lookup_string(&config, "adafruitio_key", &aioKey) == CONFIG_TRUE)
                lc->aioKey = aioKey;

            syslog(LOG_INFO, "Access to AdafruiIO is enabled to %s on port %d", lc->aioServer.c_str(), lc->aioPort);
            fprintf(stderr, "Access to AdafruiIO is enabled to %s on port %d\n", lc->aioServer.c_str(), lc->aioPort);
        }
        else  {
            syslog(LOG_INFO, "Access to AdafruitIO is disabled");
            fprintf(stderr, "Access to AdafruitIO is disabled\n");
        }
        
        if (config_lookup_int(&config, "enable_mqtt", &mqttEnabled) == CONFIG_TRUE) {
            if (mqttEnabled == 0)
                lc->mqttEnabled = false;
            else
                lc->mqttEnabled = true;
        }
        else
            lc->mqttEnabled = false;

        if (lc->mqttEnabled) {
            if (config_lookup_int(&config, "mqtt_port", &mqttPort) == CONFIG_TRUE)
                lc->mqttPort = mqttPort;
            else
                lc->mqttPort = 1883;
            
            if (config_lookup_string(&config, "mqtt_server", &mqttServer) == CONFIG_TRUE) {
                lc->mqttServer = mqttServer;
                if (lc->mqttServer.size() == 0) {
                    lc->mqttServer = "localhost";
                }
            }
            if (config_lookup_string(&config, "mqtt_user_name", &mqttUserName) == CONFIG_TRUE)
                lc->mqttUserName = mqttUserName;
            
            if (config_lookup_string(&config, "mqtt_password", &mqttPassword) == CONFIG_TRUE)
                lc->mqttPassword = mqttPassword;

            syslog(LOG_INFO, "Access to local MQTT is enabled to %s on port %d", lc->mqttServer.c_str(), lc->mqttPort);
            fprintf(stderr, "Access to local MQTT is enabled to %s on port %d\n", lc->mqttServer.c_str(), lc->mqttPort);
        }
        else {
            syslog(LOG_INFO, "Access to local MQTT is disabled");
            fprintf(stderr, "Access to local MQTT is disabled\n");
        }
        
        if (config_lookup_string(&config, "onewire_device", &wiredevice) == CONFIG_TRUE)
            lc->tempDevice = wiredevice;
        
        if (config_lookup_int(&config, "onewire_pin", &owpin) == CONFIG_TRUE)
            lc->onewirepin = owpin;
        else
            lc->onewirepin = 1;
 
        if (lc->tempDevice.size() > 0) {
            syslog(LOG_INFO, "DS18B20 device on pin %d using device name %s", lc->onewirepin, lc->tempDevice.c_str());
            fprintf(stderr, "DS18B20 device on pin %d using device name %s\n", lc->onewirepin, lc->tempDevice.c_str());
        }
        else {
            syslog(LOG_INFO, "DS18B20 device on pin %d", lc->onewirepin);
            fprintf(stderr, "DS18B20 device on pin %d\n", lc->onewirepin);
        }
            
        if (config_lookup_int(&config, "flowrate_pin", &frpin) == CONFIG_TRUE)
            lc->flowRatePin = frpin;
        else
            lc->flowRatePin = 19;
        
        syslog(LOG_INFO, "Monitoring flowrate on pin %d", lc->flowRatePin);
        fprintf(stderr, "Monitoring flowrate on pin %d\n", lc->flowRatePin);
        
        if (config_lookup_int(&config, "adc_address", &adc) == CONFIG_TRUE)
            lc->adc_address = adc;

        if (config_lookup_int(&config, "adc_channel", &channel) == CONFIG_TRUE) {
            switch (channel) {
                case 1:
                    lc->adc_channel = MCP3424_CHANNEL_1;
                    break;
                case 2:
                    lc->adc_channel = MCP3424_CHANNEL_2;
                    break;
                case 3:
                    lc->adc_channel = MCP3424_CHANNEL_3;
                    break;
                case 4:
                    lc->adc_channel = MCP3424_CHANNEL_4;
                    break;
                default:
                    lc->adc_channel = MCP3424_CHANNEL_1;
                    break;
            }
        }

        syslog(LOG_INFO, "ADC device on i2c address %x, monitoring channel %d", lc->adc_address, static_cast<int>(lc->adc_channel));
        fprintf(stderr, "ADC device on i2c address %x, monitoring channel %d\n", lc->adc_address, static_cast<int>(lc->adc_channel));
        
        if (config_lookup_int(&config, "o2sensor_address", &o2sensor) == CONFIG_TRUE)
            lc->o2sensor_address = o2sensor;
        else
            lc->o2sensor_address = 0x61;

        if (config_lookup_int(&config, "phsensor_address", &phsensor) == CONFIG_TRUE)
            lc->phsensor_address = phsensor;
        else
            lc->phsensor_address = 0x63;

        syslog(LOG_INFO, "PH device on i2c address %x", lc->phsensor_address);
        fprintf(stderr, "PH device on i2c address %x\n", lc->phsensor_address);
        syslog(LOG_INFO, "O2 device on i2c address %x", lc->o2sensor_address);
        fprintf(stderr, "O2 device on i2c address %x\n", lc->o2sensor_address);
        
        if (config_lookup_string(&config, "debug", &debug) == CONFIG_TRUE) {
            if (cisCompare(debug, "INFO"))
                setlogmask(LOG_UPTO (LOG_INFO));
            if (cisCompare(debug, "ERROR"))
                setlogmask(LOG_UPTO (LOG_ERR));
            
            syslog(LOG_INFO, "Resetting syslog debug level to %s", debug);
            fprintf(stderr, "Resetting syslog debug level to %s\n", debug);
        }
    }

    lc->oxygen = new DissolvedOxygen(0, lc->o2sensor_address);
    lc->ph = new PotentialHydrogen (0, lc->phsensor_address);
    lc->oxygen->setCallback(doCallback);
    lc->ph->setCallback(phCallback);

    config_destroy(&config);
}

void flowRateCallback(GpioInterrupt::MetaData *md)
{
    
}

/**
 * \func void sendFlowRateData(struct LocalConfig &lc)
 * \param lc LocalConfig which containst the FlowRate pointer
 * 
 * This will send Flow data to MQTT once a second, and data to aio
 * once a minute. We do a little game to allow the sensor to warmup
 * first and allow for a full minute of counts to provide some meaningful
 * data. To do this, we allow a 61 second warmup period first before
 * we send anything. Then, every 60 seconds after that, we send to
 * Adafruit. We send to MQTT every second after 61 seconds.
 */
void sendFlowRateData(const struct LocalConfig &lc)
{
    static int count = 0;
    static bool warmup = true;
    std::string payload;
    std::string aio = std::to_string(lc.fr->gpm());

    payload.append("{\"gpm\":\"" + std::to_string(lc.fr->gpm()) + "\",\n");
    payload.append("{\"lpm\":\"" + std::to_string(lc.fr->lpm()) + "\",\n");
    payload.append("{\"hertz\":\"" + std::to_string(lc.fr->hertz()) + "\"}\n");

    if (count++ > 60) {
        g_aio->publish(NULL, AIO_FLOWRATE_FEED, aio.size(), aio.c_str());
        count = 1;
        
        if (warmup)
            warmup = false;
    }
    if (!warmup)
        g_mqtt->publish(NULL, "aquarium/flowrate", payload.size(), payload.c_str());
}

void sendTempData(const struct LocalConfig &lc)
{
    static int count = 0;
    double temperature;
    std::string payload;
    std::string aio;
    
    if (lc.mqttEnabled && lc.aioEnabled) {
        lc.temp->getTemperature(temperature);
        aio = std::to_string(temperature);
        payload.append("{\"temperature\":\"" + aio + "\"\n");
        
        if (count++ > 60) {
            g_aio->publish(NULL, AIO_TEMP_FEED, aio.size(), aio.c_str());
            count = 1;
        }
        g_mqtt->publish(NULL, "aquarium/temperature", payload.size(), payload.c_str());
    }
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

bool openMCP3424(struct LocalConfig &lc)
{
    int fd = 0;
    
    fd = open("/dev/i2c-0", O_RDWR);
	if (fd == -1) {
		syslog(LOG_ERR, "error: open: %s\n", strerror(errno));
		return false;
	}

	mcp3424_init(lc.adc, fd, lc.adc_address, MCP3424_RESOLUTION_14);
    mcp3424_set_conversion_mode(lc.adc, MCP3424_CONVERSION_MODE_CONTINUOUS);
    return true;
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

	if ((config.configFile.find("$HOME") == 0) || (config.configFile.at(0) == '~')) {
        const char* homeDir = getenv("HOME");
        if (config.configFile.at(0) == '~') {
            config.configFile.erase(0, 1);
            config.configFile.insert(0, homeDir);
        }
        if (config.configFile.find("$HOME") == 0) {
            config.configFile.erase(0, 5);
            config.configFile.insert(0, homeDir);
        }
        std::cerr << __FUNCTION__ << ": Changing config file path to " << config.configFile << std::endl;
    }

    return rval;
}

void startOneWire(struct LocalConfig const &lc)
{
    std::string program = "insmod w1-gpio-custom bus0=0,";
    
    program += std::to_string(lc.onewirepin);
    program += ",0";
    
    system(program.c_str());
}

void getWaterLevel(struct LocalConfig &lc)
{
    static int count = 0;
    unsigned int result;
    std::string payload;
    std::string aio;
    
    result = mcp3424_get_raw(lc.adc, lc.adc_channel);
    
    payload += "{\"waterlevel\":";
    payload += std::to_string(result);
    payload += "}";
    
    if (count++ > 60) {
        aio = std::to_string(result);
        g_aio->publish(NULL, AIO_LEVEL_FEED, aio.size(), aio.c_str());
        count = 1;
    }
    g_mqtt->publish(NULL, "aquarium/level", payload.size(), payload.c_str());
}

void mainloop(struct LocalConfig &lc)
{
    ITimer doUpdate;
    ITimer phUpdate;
    ITimer frUpdate;
    ITimer tempUpdate;
    auto phfunc = [lc]() { lc.ph->sendStatusCommand(); };
    auto dofunc = [lc]() { lc.oxygen->sendStatusCommand(); };
    auto frfunc = [lc]() { sendFlowRateData(lc); };
    auto tempfunc = [lc]() { sendTempData(lc); };
/*    
    doUpdate.setInterval(dofunc, ONE_MINUTE);
    phUpdate.setInterval(phfunc, ONE_MINUTE);
    frUpdate.setInterval(frfunc, ONE_SECOND);
    tempUpdate.setInterval(tempfunc, ONE_SECOND);
*/    
    while (1) {
        if (lc.aioEnabled && lc.aioConnected)
            lc.g_aio->loop();
        
        if (lc.mqttEnabled && lc.mqttConnected)
            lc.g_mqtt->loop();
        
//        getWaterLevel(lc);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    doUpdate.stop();
    phUpdate.stop();
    frUpdate.stop();
    tempUpdate.stop();
}

int main(int argc, char *argv[])
{
    struct LocalConfig lc;
    std::string progname = basename(argv[0]);
    
    setlogmask(LOG_UPTO (LOG_INFO));
    openlog(progname.c_str(), LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    
    syslog(LOG_NOTICE, "Application startup");
    setConfigDefaults(lc);
    
    parse_args(argc, argv, lc);
    readConfig(&lc);


    if (lc.aioEnabled)
        lc.g_aio = new AdafruitIO(lc.localId, lc.aioServer, lc.aioUserName, lc.aioKey, lc.aioPort);
    
    if (lc.mqttEnabled)
        lc.g_mqtt = new MQTTClient(lc.localId, lc.mqttServer, lc.mqttPort);
    
/*    
    GpioInterrupt::instance()->addPin(lc.flowRatePin);
    GpioInterrupt::instance()->setPinCallback(lc.flowRatePin, flowRateCallback);
    GpioInterrupt::instance()->start(lc.flowRatePin);

    openMCP3424(lc);
*/

    lc.oxygen->sendInfoCommand();
//    lc.ph->sendInfoCommand();
    
    mainloop(lc);
    
    return 0;
}
