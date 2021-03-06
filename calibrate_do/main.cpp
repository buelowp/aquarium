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

#include <wiringPi.h>
#include <libconfig.h>
#include <syslog.h>
#include <libgen.h>
#include <errno.h>

#include "configuration.h"
#include "dissolvedoxygen.h"
#include "itimer.h"

#define ONE_SECOND          1000
#define TWO_SECONDS         (2 * ONE_SECOND)
#define ONE_MINUTE          (ONE_SECOND * 60)
#define FIFTEEN_MINUTES     (ONE_MINUTE * 15)

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

void eternalBlinkAndDie(int pin, int millihz)
{
    int state = 0;
    digitalWrite(pin, state);
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(millihz));
        state ^= 1UL << 0;
        digitalWrite(pin, state); 
    }
}

void initializeLeds()
{
    digitalWrite(Configuration::instance()->m_greenLed, 1);
    digitalWrite(Configuration::instance()->m_yellowLed, 0);
    digitalWrite(Configuration::instance()->m_redLed, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    digitalWrite(Configuration::instance()->m_greenLed, 0);
    digitalWrite(Configuration::instance()->m_yellowLed, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    digitalWrite(Configuration::instance()->m_yellowLed, 0);
    digitalWrite(Configuration::instance()->m_redLed, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    digitalWrite(Configuration::instance()->m_redLed, 0);
    digitalWrite(Configuration::instance()->m_greenLed, 1);
}

bool cisCompare(const std::string & str1, const std::string &str2)
{
    return ((str1.size() == str2.size()) && std::equal(str1.begin(), str1.end(), str2.begin(), 
            [](const char c1, const char c2){ return (c1 == c2 || std::toupper(c1) == std::toupper(c2)); }
            ));
}

void setNormalDisplay()
{
    digitalWrite(g_localConfig->green_led, 1);
    digitalWrite(g_localConfig->yellow_led, 0);
    digitalWrite(g_localConfig->red_led, 0);    
}

void setWarningDisplay()
{
    digitalWrite(g_localConfig->green_led, 0);
    digitalWrite(g_localConfig->yellow_led, 1);
    digitalWrite(g_localConfig->red_led, 0);    
}

void setErrorDisplay()
{
    digitalWrite(g_localConfig->green_led, 0);
    digitalWrite(g_localConfig->yellow_led, 0);
    digitalWrite(g_localConfig->red_led, 1);    
}

void initializeLeds(struct LocalConfig &lc)
{
    digitalWrite(lc.green_led, 1);
    digitalWrite(lc.yellow_led, 0);
    digitalWrite(lc.red_led, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    digitalWrite(lc.green_led, 0);
    digitalWrite(lc.yellow_led, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    digitalWrite(lc.yellow_led, 0);
    digitalWrite(lc.red_led, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    digitalWrite(lc.red_led, 0);
    digitalWrite(lc.green_led, 1);
}

void mqttIncomingMessage(std::string, std::string)
{
}

void mqttConnectionLost(const std::string &cause)
{
    std::cout << "MQTT disconnected: " << cause << std::endl;
    Configuration::instance()->m_mqttConnected = false;
}

void mqttConnected()
{
    std::cout << "MQTT connected!" << std::endl;
}

void aioIncomingMessage(std::string topic, std::string message)
{
    std::cout << __FUNCTION__ << ": Odd, we shouldn't get messages from AIO" << std::endl;
}

void aioConnected()
{
    std::cout << "AIO connected!" << std::endl;
    Configuration::instance()->m_aioConnected = true;
}

void aioConnectionLost(const std::string &cause)
{
    std::cout << __FUNCTION__ << ": AIO disconnected: " << cause << std::endl;
    Configuration::instance()->m_aioEnabled = false;
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
    Configuration::instance()->m_oxygen->calibrate(DissolvedOxygen::DO_DEFAULT, nullptr, 0);
    g_mutex.unlock();
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void doCallback(int cmd, std::string response)
{
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
    auto dofunc = [lc](void*) { lc.oxygen->sendReadCommand(900); };
        
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Calibration operation for the DO probe." << std::endl;
    std::cout << "This is a single point calibration routine. Expose the probe to air for 30 seconds." << std::endl;
    std::cout << "When readings stabilize, press enter to store calibration." << std::endl;
    
    initializeLeds(lc);
    
    g_localConfig->done = false;
    std::cin.ignore( std::numeric_limits <std::streamsize> ::max(), '\n' );
    Configuration::instance()->m_oxygen->sendReadCommand(600);
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
    
    syslog(LOG_NOTICE, "Dissolved Oxygen Sensor Application startup");
    wiringPiSetupGpio();
    piHiPri(99);
    
    if (!parse_args(argc, argv, lc)) {
        eternalBlinkAndDie(lc.red_led, 2000);
    }
    // if this goes badly, just die but leave the error LED blinking at 1 hz
    if (!Configuration::instance()->readConfigFile()) {
        std::cerr << "Unable to read configuration file, exiting..." << std::endl;
        syslog(LOG_ERR, "Unable to read configuration file, exiting...");
        exit(-2);
    }

    initializeLeds();
        
    g_localConfig = &lc;
    if (g_localConfig->clear) {
        std::cout << "Clearing calibration data..." << std::endl;
        Configuration::instance()->m_oxygen->calibrate(DissolvedOxygen::DO_CLEAR, nullptr, 0);
        Configuration::instance()->m_oxygen->calibrate(DissolvedOxygen::DO_QUERY, nullptr, 0);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    else if (g_localConfig->query) {
        std::cout << "Checking calibration data..." << std::endl;
        Configuration::instance()->m_oxygen->calibrate(DissolvedOxygen::DO_QUERY, nullptr, 0);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }        
    else {
        Configuration::instance()->m_oxygen->sendInfoCommand();
        Configuration::instance()->m_oxygen->calibrate(DissolvedOxygen::DO_QUERY, nullptr, 0);
        mainloop(lc);
    }
    
    return 0;
}
