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
#include <mutex>
#include <condition_variable>

#include <gpiointerruptpp.h>
#include <libconfig.h>
#include <syslog.h>
#include <libgen.h>
#include <errno.h>

#include "dissolvedoxygen.h"
#include "itimer.h"

#define DEFAULT_FR_PIN      15
#define ONE_SECOND          1000
#define TWO_SECONDS         (2 * ONE_SECOND)
#define ONE_MINUTE          (ONE_SECOND * 60)
#define FIFTEEN_MINUTES     (ONE_MINUTE * 15)

#define AIO_FLOWRATE_FEED   "pbuelow/feeds/aquarium.flowrate"
#define AIO_OXYGEN_FEED     "pbuelow/feeds/aquarium.oxygen"
#define AIO_PH_FEED         "pbuelow/feeds/aquarium.ph"
#define AIO_TEMP_FEED       "pbuelow/feeds/aquarium.Temperature"
#define AIO_LEVEL_FEED      "pbuelow/feeds/aquarium.waterlevel"

#define GREEN_LED           13
#define YELLOW_LED          12
#define RED_LED             18

struct LocalConfig {
    DissolvedOxygen *oxygen;
    std::string configFile;
    int dosensor_address;
    int red_led;
    int yellow_led;
    int green_led;
    int operation;
    bool done;
    bool clear;
    bool query;
};

struct LocalConfig *g_localConfig;
std::mutex g_mutex;

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

void waitForInput()
{
    while (!g_localConfig->done) {
        g_mutex.lock();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        g_mutex.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void writeCalibrationData()
{
    g_mutex.lock();
    g_localConfig->oxygen->calibrate(DissolvedOxygen::DEFAULT, nullptr, 0);
    g_mutex.unlock();
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void doCallback(int cmd, std::string response)
{
    int calibration = 0;
    
    switch (cmd) {
        case AtlasScientificI2C::INFO:
            break;
        case AtlasScientificI2C::CALIBRATE:
            if (response.find("?CAL") != std::string::npos) {
                std::cout << "There are " << response.substr(6) << " points of calibration" << std::endl;
                if (response.substr(6) == "0") {
                    setErrorDisplay();
                }
                if (response.substr(6) == "1") {
                    setNormalDisplay();
                }
            }
            break;
        case AtlasScientificI2C::READING:
            std::cout << "DO: " << response << '\r' << std::flush;
            break;
        default:
            break;
    }
}

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

bool cisCompare(const std::string & str1, const std::string &str2)
{
	return ((str1.size() == str2.size()) && std::equal(str1.begin(), str1.end(), str2.begin(), 
            [](const char c1, const char c2){ return (c1 == c2 || std::toupper(c1) == std::toupper(c2)); }
            ));
}

void setConfigDefaults(struct LocalConfig &lc)
{
    lc.configFile = "~/.config/aquarium.conf";
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
    
    config_init(&config);
    if (config_read_file(&config, lc->configFile.c_str()) == CONFIG_FALSE) {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&config), config_error_line(&config), config_error_text(&config));
        config_destroy(&config);
        setErrorDisplay();
        return false;
    }
    else {
 
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


        if (config_lookup_int(&config, "dosensor_address", &o2sensor) == CONFIG_TRUE)
            lc->dosensor_address = phsensor;
        else
            lc->dosensor_address = 0x63;

        syslog(LOG_INFO, "DO device on i2c address %x", lc->dosensor_address);
        fprintf(stderr, "DO device on i2c address %x\n", lc->dosensor_address);
        
        if (config_lookup_string(&config, "debug", &debug) == CONFIG_TRUE) {
            if (cisCompare(debug, "INFO"))
                setlogmask(LOG_UPTO (LOG_INFO));
            if (cisCompare(debug, "ERROR"))
                setlogmask(LOG_UPTO (LOG_ERR));
            
            syslog(LOG_INFO, "Resetting syslog debug level to %s", debug);
            fprintf(stderr, "Resetting syslog debug level to %s\n", debug);
        }
    }

    lc->oxygen = new DissolvedOxygen(0, lc->dosensor_address);
    lc->oxygen->setCallback(doCallback);
    
    config_destroy(&config);
    return true;
}

void usage(const char *name)
{
    std::cerr << "usage: " << name << " -h <server> -p <port> -n <unique id> -u <username> -k <password/key> -d" << std::endl;
    std::cerr << "\t-c alternate configuration file (defaults to $HOME/.config/aquarium.conf" << std::endl;
    std::cerr << "\t-l Clear calibration data and exit" << std::endl;
    std::cerr << "\t-q Query calibration state and exit" << std::endl;
    std::cerr << "\t-z Zero calibration and exit" << std::endl;
    std::cerr << "\t-h Print usage and exit" << std::endl;
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
    
    config.clear = false;
    config.query = false;
	if (argv) {
		while ((opt = getopt(argc, argv, "c:hlq")) != -1) {
			switch (opt) {
            case 'h':
                usage(argv[0]);
                break;
			case 'c':
				config.configFile = optarg;
				break;
            case 'l':
                config.clear = true;
                break;
            case 'q':
                config.query = true;
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

void mainloop(struct LocalConfig &lc)
{
    ITimer oxygen;
    auto dofunc = [lc]() { lc.oxygen->sendReadCommand(900); };
        
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Calibration operation for the DO probe." << std::endl;
    std::cout << "This is a single point calibration routine. Expose the probe to air for 30 seconds." << std::endl;
    std::cout << "When readings stabilize, press enter to store calibration." << std::endl;
    
    initializeLeds(lc);
    
    g_localConfig->done = false;
    std::cin.ignore( std::numeric_limits <std::streamsize> ::max(), '\n' );
    g_localConfig->oxygen->sendReadCommand(600);
    std::thread listener(waitForInput);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::thread sender(writeCalibrationData);
    oxygen.setInterval(dofunc, TWO_SECONDS);
    sender.join();
    g_localConfig->done = true;
    std::cout << "Calibration complete, cleaning up, press enter to start cleaning up" << std::endl;
    oxygen.stop();
    listener.join();
    std::this_thread::sleep_for(std::chrono::seconds(2));
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
    
//    GpioInterrupt::instance()->start();    
    g_localConfig = &lc;
    if (g_localConfig->clear) {
        std::cout << "Clearing calibration data..." << std::endl;
        g_localConfig->oxygen->calibrate(DissolvedOxygen::CLEAR, nullptr, 0);
        g_localConfig->oxygen->calibrate(DissolvedOxygen::QUERY, nullptr, 0);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    else if (g_localConfig->query) {
        std::cout << "Checking calibration data..." << std::endl;
        g_localConfig->oxygen->calibrate(DissolvedOxygen::QUERY, nullptr, 0);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }        
    else {
        g_localConfig->oxygen->sendInfoCommand();
        g_localConfig->oxygen->calibrate(DissolvedOxygen::QUERY, nullptr, 0);
        mainloop(lc);
    }
    
    return 0;
}
