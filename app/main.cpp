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
#include <iomanip>

#include <gpiointerruptpp.h>
#include <adaio.h>
#include <libconfig.h>
#include <syslog.h>
#include <libgen.h>
#include <errno.h>
#include <mcp3424.h>
#include <nlohmann/json.hpp>

#include "potentialhydrogen.h"
#include "dissolvedoxygen.h"
#include "mqttclient.h"
#include "flowrate.h"
#include "itimer.h"
#include "temperature.h"

#define DEFAULT_FR_PIN      15
#define ONE_SECOND          1000
#define TEN_SECONDS         (ONE_SECOND * 10)
#define ONE_MINUTE          (ONE_SECOND * 60)
#define FIFTEEN_MINUTES     (ONE_MINUTE * 15)

#define AIO_FLOWRATE_FEED   "pbuelow/feeds/aquarium.flowrate"
#define AIO_OXYGEN_FEED     "pbuelow/feeds/aquarium.oxygen"
#define AIO_PH_FEED         "pbuelow/feeds/aquarium.ph"
#define AIO_TEMP_FEED       "pbuelow/feeds/aquarium.Temperature"
#define AIO_LEVEL_FEED      "pbuelow/feeds/aquarium.waterlevel"

#define RAW_MIN             0
#define RAW_MAX_12          2048
#define RAW_MAX_14          8192
#define RAW_MAX_16          32768
#define RAW_MAX_18          131072
#define V_MIN               0.00
#define V_MAX               3.30

#define GREEN_LED           17
#define YELLOW_LED          16
#define RED_LED             15

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
    mcp3424_channel wl_channel;
    int adc_address;
    int o2sensor_address;
    int phsensor_address;
    int onewirepin;
    int red_led;
    int yellow_led;
    int green_led;
    mcp3424_resolution adc_resolution;
};

struct LocalConfig *g_localConfig;

void eternalBlinkAndDie(int pin, int millihz)
{
    int state = 0;
    GpioInterrupt::instance()->value(pin, state);
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(millihz));
        state ^= 1UL << 0;
        GpioInterrupt::instance()->setValue(pin, state); 
    }
}

void setNormalDisplay()
{
    GpioInterrupt::instance()->setValue(g_localConfig->green_led, 1);
    GpioInterrupt::instance()->setValue(g_localConfig->yellow_led, 0);
    GpioInterrupt::instance()->setValue(g_localConfig->red_led, 0);    
}

void setWarningDisplay()
{
    GpioInterrupt::instance()->setValue(g_localConfig->green_led, 0);
    GpioInterrupt::instance()->setValue(g_localConfig->yellow_led, 1);
    GpioInterrupt::instance()->setValue(g_localConfig->red_led, 0);    
}

void setErrorDisplay()
{
    GpioInterrupt::instance()->setValue(g_localConfig->green_led, 0);
    GpioInterrupt::instance()->setValue(g_localConfig->yellow_led, 0);
    GpioInterrupt::instance()->setValue(g_localConfig->red_led, 1);    
}

void initializeLeds(struct LocalConfig &lc)
{
    GpioInterrupt::instance()->setValue(lc.green_led, 1);
    GpioInterrupt::instance()->setValue(lc.yellow_led, 0);
    GpioInterrupt::instance()->setValue(lc.red_led, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    GpioInterrupt::instance()->setValue(lc.green_led, 0);
    GpioInterrupt::instance()->setValue(lc.yellow_led, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    GpioInterrupt::instance()->setValue(lc.yellow_led, 0);
    GpioInterrupt::instance()->setValue(lc.red_led, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    GpioInterrupt::instance()->setValue(lc.red_led, 0);
    GpioInterrupt::instance()->setValue(lc.green_led, 1);
}

bool cisCompare(const std::string & str1, const std::string &str2)
{
	return ((str1.size() == str2.size()) && std::equal(str1.begin(), str1.end(), str2.begin(), 
            [](const char c1, const char c2){ return (c1 == c2 || std::toupper(c1) == std::toupper(c2)); }
            ));
}

void decodeStatusResponse(std::string &response)
{
    int pos = response.find_last_of(",");
    double voltage;
    
    if (pos != std::string::npos) {
        voltage = std::stod(response.substr(pos + 1));
        if (voltage > 3)
            std::cout << "probe is operating normally, reporting voltage " << voltage;
        else 
            std::cout << "probe is reporting an unusual voltage (" << voltage << "), it may not be operating correctly.";
    }
    else {
        std::cout << "probe status cannot be decoded";
    }
}

void decodeInfoResponse(std::string &response)
{
    int pos = response.find_last_of(",");
    
    if (pos != std::string::npos) {
        std::cout << "probe is running firmware version " << response.substr(pos + 1);
    }
    else {
        std::cout << "probe info response cannot be decoded";
    }
}

void decodeTempCompensation(std::string &response)
{
    int pos = response.find_last_of(",");
    if (pos != std::string::npos) {
        std::cout << "probe has a temp compensation value of " << response.substr(pos + 1) << "C";
    }
    else {
        std::cout << "probe temp compensation response cannot be decoded";
    }
}

void phCallback(int cmd, std::string response)
{
    switch (cmd) {
        case AtlasScientificI2C::INFO:
            syslog(LOG_NOTICE, "got pH probe info event: %s\n", response.c_str());
            std::cout << "pH "; 
            decodeInfoResponse(response);
            std::cout << std::endl;
            break;
        case AtlasScientificI2C::STATUS:
            syslog(LOG_NOTICE, "got pH probe status event: %s\n", response.c_str());
            std::cout << "pH ";
            decodeStatusResponse(response);
            std::cout << std::endl;
            break;
        case AtlasScientificI2C::CALIBRATE:
            if (response.find(",0") != std::string::npos)
                std::cout << "pH probe reports no calibration data..." << std::endl;
            break;
        case AtlasScientificI2C::READING:
//            std::cout << "PH Value response " << response << std::endl;
            break;
        case AtlasScientificI2C::GETTEMPCOMP:
            std::cout << "pH ";
            decodeTempCompensation(response);
            std::cout << std::endl;
        default:
            break;
    }
}

void doCallback(int cmd, std::string response)
{
    switch (cmd) {
        case AtlasScientificI2C::INFO:
            syslog(LOG_NOTICE, "got DO probe info event: %s\n", response.c_str());
            std::cout << "DO ";
            decodeInfoResponse(response);
            std::cout << std::endl;
            break;
        case AtlasScientificI2C::STATUS:
            syslog(LOG_NOTICE, "got DO probe status event: %s\n", response.c_str());
            std::cout << "DO ";
            decodeStatusResponse(response);
            std::cout << std::endl;
            break;
        case AtlasScientificI2C::CALIBRATE:
            if (response.find(",0") != std::string::npos)
                std::cout << "DO probe reports no calibration data..." << std::endl;
            break;
        case AtlasScientificI2C::GETTEMPCOMP:
            std::cout << "pH ";
            decodeTempCompensation(response);
            std::cout << std::endl;
        default:
            break;
    }
}

void setConfigDefaults(struct LocalConfig &lc)
{
    lc.configFile = "~/.config/aquarium.conf";
    lc.daemonize = false;
    lc.fr = new FlowRate();
    lc.adc = new mcp3424();
    lc.temp = nullptr;
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
    int red;
    int yellow;
    int green;
    int resolution;
    
    config_init(&config);
    if (config_read_file(&config, lc->configFile.c_str()) == CONFIG_FALSE) {
//        syslog(LOG_ERR, "config_read: error in file %s, line %d\n", config_error_file(&config), config_error_line(&config));
//        std::cerr << __FUNCTION__ << ": config_read: error in file " << config_error_file(&config) << ", line " << config_error_line(&config) << std::endl;
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&config), config_error_line(&config), config_error_text(&config));
        config_destroy(&config);
        setErrorDisplay();
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
 
        if (config_lookup_int(&config, "red_led", &red) == CONFIG_TRUE)
            lc->red_led = red;
        else
            lc->red_led = RED_LED;

        if (config_lookup_int(&config, "yellow_led", &yellow) == CONFIG_TRUE)
            lc->yellow_led = yellow;
        else
            lc->yellow_led = YELLOW_LED;

        if (config_lookup_int(&config, "green_led", &green) == CONFIG_TRUE)
            lc->green_led = green;
        else
            lc->green_led = GREEN_LED;

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

        if (config_lookup_int(&config, "water_level_channel", &channel) == CONFIG_TRUE) {
            switch (channel) {
                case 1:
                    lc->wl_channel = MCP3424_CHANNEL_1;
                    break;
                case 2:
                    lc->wl_channel = MCP3424_CHANNEL_2;
                    break;
                case 3:
                    lc->wl_channel = MCP3424_CHANNEL_3;
                    break;
                case 4:
                    lc->wl_channel = MCP3424_CHANNEL_4;
                    break;
                default:
                    lc->wl_channel = MCP3424_CHANNEL_2;
                    break;
            }
        }

        if (config_lookup_int(&config, "adc_resolution", &resolution) == CONFIG_TRUE) {
            switch (channel) {
                case 12:
                    lc->adc_resolution = MCP3424_RESOLUTION_12;
                    break;
                case 14:
                    lc->adc_resolution = MCP3424_RESOLUTION_14;
                    break;
                case 16:
                    lc->adc_resolution = MCP3424_RESOLUTION_16;
                    break;
                case 18:
                    lc->adc_resolution = MCP3424_RESOLUTION_18;
                    break;
                default:
                    lc->adc_resolution = MCP3424_RESOLUTION_12;
                    break;
            }
        }
        else {
            lc->adc_resolution = MCP3424_RESOLUTION_12;
        }

        syslog(LOG_INFO, "ADC device on i2c address %x, monitoring channel %d, %d bits resolution", lc->adc_address, static_cast<int>(lc->wl_channel), static_cast<int>(lc->adc_resolution));
        fprintf(stderr, "ADC device on i2c address %x, monitoring channel %d, %d bits resolution\n", lc->adc_address, channel, static_cast<int>(lc->adc_resolution));
        
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
    nlohmann::json j;
    std::string aio = std::to_string(lc.fr->gpm());

    j["aquarium"]["flowrate"]["gpm"] = lc.fr->gpm();
    j["aquarium"]["flowrate"]["lpm"] = lc.fr->lpm();
    j["aquarium"]["flowrate"]["hertz"] = lc.fr->hertz();
    
    std::cout << j.dump(2) << std::endl;
    /*
    if (count++ > 60) {
        lc.g_aio->publish(NULL, AIO_FLOWRATE_FEED, aio.size(), aio.c_str());
        count = 1;
        
        if (warmup)
            warmup = false;
    }
    if (!warmup)
        g_mqtt->publish(NULL, "aquarium/flowrate", payload.size(), payload.c_str());
    */
}

void sendTempData(const struct LocalConfig &lc)
{
    static int count = 0;
    double tc;
    double tf;
    std::string aio;
    nlohmann::json j;
    
    if (!lc.mqttEnabled && !lc.aioEnabled) {
        if (lc.temp) {
            lc.temp->getTemperature(tc, tf);
            j["aquarium"]["temperature"]["celsius"] = tc;
            j["aquarium"]["temperature"]["farenheit"] = tf;
            std::cout << j.dump(4) << std::endl;
        }
/*        
        if (count++ > 60) {
            lc.g_aio->publish(NULL, AIO_TEMP_FEED, aio.size(), aio.c_str());
            count = 1;
        }
        g_mqtt->publish(NULL, "aquarium/temperature", payload.size(), payload.c_str());
        */
    }
}

void aioGenericCallback(AdafruitIO::CallbackType type, int code)
{
    switch (type) {
        case AdafruitIO::CallbackType::CONNECT:
            syslog(LOG_INFO, "AdafruitIO connected");
            break;
        case AdafruitIO::CallbackType::DISCONNECT:
            syslog(LOG_NOTICE, "AdafruitIO disconnect event: %d", code);
            break;
        default:
            break;
    }
}

void mqttGenericCallback(MQTTClient::CallbackType type, int code)
{
    switch (type) {
        case MQTTClient::CallbackType::CONNECT:
            syslog(LOG_INFO, "MQTT connected");
            break;
        case MQTTClient::CallbackType::DISCONNECT:
            syslog(LOG_NOTICE, "MQTT disconnect event: %d", code);
            break;
        default:
            break;
    }
}

bool openMCP3424(struct LocalConfig &lc)
{
    int fd = 0;
    
    fd = open("/dev/i2c-0", O_RDWR);
	if (fd == -1) {
		syslog(LOG_ERR, "error: open: %s\n", strerror(errno));
        std::cerr << "open: " << strerror(errno) << std::endl;
        setWarningDisplay();
		return false;
	}

	mcp3424_init(lc.adc, fd, lc.adc_address, lc.adc_resolution);
    mcp3424_set_conversion_mode(lc.adc, MCP3424_CONVERSION_MODE_CONTINUOUS);
    fprintf(stdout, "MCP 3424 setup complete for address 0x%x, fd %d\n", lc.adc_address, lc.adc->fd);
    syslog(LOG_INFO, "MCP 3424 setup complete for address 0x%x, fd %d", lc.adc_address, lc.adc->fd);
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
                return false;
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

void startOneWire(struct LocalConfig &lc)
{
    std::string program = "insmod w1-gpio-custom bus0=0,";
    
    program += std::to_string(lc.onewirepin);
    program += ",0";
    
    std::cout << "Executing: " << program << std::endl;
    system(program.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    lc.temp = new Temperature();
    
    if (!lc.temp->enabled()) {
        setWarningDisplay();
    }
}

void getWaterLevel(const struct LocalConfig &lc)
{
    static int count = 0;
    unsigned int result;
    std::string aio;
    nlohmann::json j;
    
    result = mcp3424_get_raw(lc.adc, lc.wl_channel);
    if (lc.adc->err) {
        syslog(LOG_ERR, "%s:%d: Unable to get data on channel %d: %s", __FUNCTION__, __LINE__, lc.wl_channel, lc.adc->errstr);
        std::cerr << "Unable to get data, error " << lc.adc->errstr << std::endl;
    }
    else {
        j["aquarium"]["waterlevel"] = result;
        std::cout << j.dump(4) << std::endl;
    /*
    if (count++ > 60) {
        aio = std::to_string(result);
        lc.g_aio->publish(NULL, AIO_LEVEL_FEED, aio.size(), aio.c_str());
        count = 1;
    }
    lc.g_mqtt->publish(NULL, "aquarium/level", payload.size(), payload.c_str());
    */
    }
}

void sendResultData(const struct LocalConfig &lc)
{
    nlohmann::json j;
    unsigned int result;
    double tc;
    double tf;

    result = mcp3424_get_raw(lc.adc, lc.wl_channel);
    if (lc.adc->err) {
        syslog(LOG_ERR, "%s:%d: Unable to get data on channel %d: %s", __FUNCTION__, __LINE__, lc.wl_channel, lc.adc->errstr);
        std::cerr << "Unable to get data, error " << lc.adc->errstr << std::endl;
    }
    else {
        j["aquarium"]["waterlevel"] = result;
    }
    if (lc.temp) {
        lc.temp->getTemperature(tc, tf);
        j["aquarium"]["temperature"]["celsius"] = tc;
        j["aquarium"]["temperature"]["farenheit"] = tf;
    }
    j["aquarium"]["flowrate"]["gpm"] = lc.fr->gpm();
    j["aquarium"]["flowrate"]["lpm"] = lc.fr->lpm();
    j["aquarium"]["flowrate"]["hertz"] = lc.fr->hertz();
    j["aquarium"]["ph"] = lc.ph->getPH();
    j["aquarium"]["oxygen"] = lc.oxygen->getDO();
    std::cout << j.dump(4) << std::endl;
    if (lc.mqttEnabled && lc.mqttConnected) {
        lc.g_mqtt->publish(NULL, "aquarium/data", j.dump().size(), j.dump().c_str());
    }
    if (lc.aioEnabled && lc.aioConnected) {
        lc.g_aio->publish(NULL, AIO_LEVEL_FEED, j.dump().size(), j.dump().c_str());
    }
}

void setTempCompensation(const struct LocalConfig &lc)
{
    double tc = lc.temp->celsius();

    if (tc != 0) {
        lc.ph->setTempCompensationAndRead(tc);
        lc.oxygen->setTempCompensationAndRead(tc);
    }
}

void mainloop(struct LocalConfig &lc)
{
    ITimer doUpdate;
    ITimer phUpdate;
    ITimer sendUpdate;
    ITimer tempCompensation;
    
    auto phfunc = [lc]() { lc.ph->sendReadCommand(900); };
    auto dofunc = [lc]() { lc.oxygen->sendReadCommand(600); };
    auto updateFunc = [lc]() { sendResultData(lc); };
    auto compFunc = [lc]() { setTempCompensation(lc); };
    
    doUpdate.setInterval(dofunc, TEN_SECONDS);
    phUpdate.setInterval(phfunc, TEN_SECONDS);
    sendUpdate.setInterval(updateFunc, ONE_MINUTE);
    tempCompensation.setTimeout(compFunc, FIFTEEN_MINUTES);
    
    setTempCompensation(lc);
    
    while (1) {
        if (lc.aioEnabled && lc.aioConnected)
            lc.g_aio->loop();
        
        if (lc.mqttEnabled && lc.mqttConnected)
            lc.g_mqtt->loop();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    doUpdate.stop();
    phUpdate.stop();
    sendUpdate.stop();
}

int main(int argc, char *argv[])
{
    struct LocalConfig lc;
    std::string progname = basename(argv[0]);
    
    setlogmask(LOG_UPTO (LOG_INFO));
    openlog(progname.c_str(), LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    
    syslog(LOG_NOTICE, "Application startup");
    setConfigDefaults(lc);
    
    if (!parse_args(argc, argv, lc)) {
        eternalBlinkAndDie(lc.red_led, 2000);
    }
    // if this goes badly, just die but leave the error LED blinking at 1 hz
    if (!readConfig(&lc)) {
        eternalBlinkAndDie(lc.red_led, 1000);
    }

    if (!GpioInterrupt::instance()->addPin(lc.green_led, GpioInterrupt::GPIO_DIRECTION_OUT, GpioInterrupt::GPIO_IRQ_NONE))
        syslog(LOG_ERR, "Unable to open GPIO for green led");
    
    if (!GpioInterrupt::instance()->addPin(lc.yellow_led, GpioInterrupt::GPIO_DIRECTION_OUT, GpioInterrupt::GPIO_IRQ_NONE))
        syslog(LOG_ERR, "Unable to open GPIO for yellow led");

    if (!GpioInterrupt::instance()->addPin(lc.red_led, GpioInterrupt::GPIO_DIRECTION_OUT, GpioInterrupt::GPIO_IRQ_NONE))
        syslog(LOG_ERR, "Unable to open GPIO for red led");

    initializeLeds(lc);

    GpioInterrupt::instance()->addPin(lc.flowRatePin);
    GpioInterrupt::instance()->setPinCallback(lc.flowRatePin, flowRateCallback);
        
    startOneWire(lc);
    
    if (lc.aioEnabled)
        lc.g_aio = new AdafruitIO(lc.localId, lc.aioServer, lc.aioUserName, lc.aioKey, lc.aioPort);
    
    if (lc.mqttEnabled)
        lc.g_mqtt = new MQTTClient(lc.localId, lc.mqttServer, lc.mqttPort);
    
    GpioInterrupt::instance()->start();

    openMCP3424(lc);
    
    g_localConfig = &lc;

    lc.oxygen->sendInfoCommand();
    lc.oxygen->calibrate(DissolvedOxygen::QUERY, nullptr, 0);
    lc.oxygen->getTempCompensation();
    lc.oxygen->sendStatusCommand();

    lc.ph->sendInfoCommand();
    lc.ph->calibrate(PotentialHydrogen::QUERY, nullptr, 0);
    lc.ph->getTempCompensation();
    lc.ph->sendStatusCommand();
    
    mainloop(lc);
    
    return 0;
}
