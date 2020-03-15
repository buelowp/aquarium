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
#include <ctime>
#include <sstream>
#include <mutex>
#include <condition_variable>

#include <gpiointerruptpp.h>
#include <adaio.h>
#include <syslog.h>
#include <libgen.h>
#include <errno.h>
#include <nlohmann/json.hpp>

#include "potentialhydrogen.h"
#include "dissolvedoxygen.h"
#include "flowrate.h"
#include "itimer.h"
#include "temperature.h"
#include "mcp3008.h"
#include "configuration.h"
#include "errorhandler.h"
#include "fatal.h"
#include "critical.h"
#include "warning.h"
#include "localmqttcallback.h"

#define ONE_SECOND          1000
#define TEN_SECONDS         (ONE_SECOND * 10)
#define ONE_MINUTE          (ONE_SECOND * 60)
#define FIVE_MINUTES        (ONE_MINUTE * 5)
#define FIFTEEN_MINUTES     (ONE_MINUTE * 15)
#define ONE_HOUR            (ONE_MINUTE * 60)

#define AIO_FLOWRATE_FEED   "pbuelow/feeds/aquarium.flowrate"
#define AIO_OXYGEN_FEED     "pbuelow/feeds/aquarium.oxygen"
#define AIO_PH_FEED         "pbuelow/feeds/aquarium.ph"
#define AIO_TEMP_FEED       "pbuelow/feeds/aquarium.Temperature"
#define AIO_LEVEL_FEED      "pbuelow/feeds/aquarium.waterlevel"

ErrorHandler g_errors;
std::mutex g_mqttMutex;
std::condition_variable g_mqttCV;

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

void initializeLeds()
{
    GpioInterrupt::instance()->setValue(Configuration::instance()->m_greenLed, 1);
    GpioInterrupt::instance()->setValue(Configuration::instance()->m_yellowLed, 0);
    GpioInterrupt::instance()->setValue(Configuration::instance()->m_redLed, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    GpioInterrupt::instance()->setValue(Configuration::instance()->m_greenLed, 0);
    GpioInterrupt::instance()->setValue(Configuration::instance()->m_yellowLed, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    GpioInterrupt::instance()->setValue(Configuration::instance()->m_yellowLed, 0);
    GpioInterrupt::instance()->setValue(Configuration::instance()->m_redLed, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    GpioInterrupt::instance()->setValue(Configuration::instance()->m_redLed, 0);
    GpioInterrupt::instance()->setValue(Configuration::instance()->m_greenLed, 1);
}

bool cisCompare(const std::string & str1, const std::string &str2)
{
    return ((str1.size() == str2.size()) && std::equal(str1.begin(), str1.end(), str2.begin(), 
            [](const char c1, const char c2){ return (c1 == c2 || std::toupper(c1) == std::toupper(c2)); }
            ));
}

void decodeStatusResponse(std::string which, std::string &response)
{
    mqtt::message_ptr pubmsg;
    nlohmann::json j;
    int pos = response.find_last_of(",");
    double voltage;
    
    if (pos != std::string::npos) {
        voltage = std::stod(response.substr(pos + 1));
        if (voltage > 3) {
            std::cout << "probe is operating normally, reporting voltage " << voltage;
        }
        else 
            std::cout << "probe is reporting an unusual voltage (" << voltage << "), it may not be operating correctly.";
        if (which == "pH") {
            if (Configuration::instance()->m_mqtt->is_connected()) {
                j["aquarium"]["device"]["ph"]["voltage"] = response.substr(pos + 1);
                pubmsg = mqtt::make_message("aquarium/device", j.dump());
                if (Configuration::instance()->m_mqttConnected)
                    Configuration::instance()->m_mqtt->publish(pubmsg);
            }
            Configuration::instance()->m_phVoltage = response.substr(pos + 1);
        }
        else if (which == "DO") {
            if (Configuration::instance()->m_mqtt->is_connected()) {
                j["aquarium"]["device"]["dissolvedoxygen"]["version"] = response.substr(pos + 1);
                pubmsg = mqtt::make_message("aquarium/device", j.dump());
                if (Configuration::instance()->m_mqttConnected)
                    Configuration::instance()->m_mqtt->publish(pubmsg);
            }
            Configuration::instance()->m_o2Voltage = response.substr(pos + 1);
        }
    }
    else {
        std::cout << "probe status cannot be decoded";
    }
}

void decodeInfoResponse(std::string which, std::string &response)
{
    mqtt::message_ptr pubmsg;
    nlohmann::json j;
    int pos = response.find_last_of(",");
    
    if (pos != std::string::npos) {
        std::cout << "probe is running firmware version " << response.substr(pos + 1);
        if (which == "pH") {
            if (Configuration::instance()->m_mqtt->is_connected()) {
                j["aquarium"]["device"]["ph"]["version"] = response.substr(pos + 1);
                pubmsg = mqtt::make_message("aquarium/device", j.dump());
                Configuration::instance()->m_mqtt->publish(pubmsg);
            }
            Configuration::instance()->m_phVersion = response.substr(pos + 1);
        }
        else if (which == "DO") {
            if (Configuration::instance()->m_mqtt->is_connected()) {
                j["aquarium"]["device"]["dissolvedoxygen"]["version"] = response.substr(pos + 1);
                pubmsg = mqtt::make_message("aquarium/device", j.dump());
                Configuration::instance()->m_mqtt->publish(pubmsg);
            }
            Configuration::instance()->m_o2Version = response.substr(pos + 1);
        }
    }
    else {
        std::cout << "probe info response cannot be decoded";
    }
}

void decodeTempCompensation(std::string which, std::string &response)
{
    mqtt::message_ptr pubmsg;
    nlohmann::json j;

    int pos = response.find_last_of(",");
    if (pos != std::string::npos) {
        std::cout << "probe has a temp compensation value of " << response.substr(pos + 1) << "C";
        if (which == "pH") {
            if (Configuration::instance()->m_mqtt->is_connected()) {
                j["aquarium"]["device"]["ph"]["tempcompensation"] = response.substr(pos + 1);
                pubmsg = mqtt::make_message("aquarium/device", j.dump());
                Configuration::instance()->m_mqtt->publish(pubmsg);
            }
            Configuration::instance()->m_o2TempComp = response.substr(pos + 1);
        }
        else if (which == "DO") {
            if (Configuration::instance()->m_mqtt->is_connected()) {
                j["aquarium"]["device"]["dissolvedoxygen"]["tempcompensation"] = response.substr(pos + 1);
                pubmsg = mqtt::make_message("aquarium/device", j.dump());
                Configuration::instance()->m_mqtt->publish(pubmsg);
            }
            Configuration::instance()->m_o2TempComp = response.substr(pos + 1);
        }
    }
    else {
        std::cout << "probe temp compensation response cannot be decoded: " << response;
    }
}

void phCallback(int cmd, std::string response)
{
    switch (cmd) {
        case AtlasScientificI2C::INFO:
            syslog(LOG_NOTICE, "got pH probe info event: %s\n", response.c_str());
            std::cout << "pH "; 
            decodeInfoResponse("pH", response);
            std::cout << std::endl;
            break;
        case AtlasScientificI2C::STATUS:
            syslog(LOG_NOTICE, "got pH probe status event: %s\n", response.c_str());
            std::cout << "pH ";
            decodeStatusResponse("pH", response);
            std::cout << std::endl;
            break;
        case AtlasScientificI2C::CALIBRATE:
            if (response.find(",0") != std::string::npos)
                std::cout << "pH probe reports no calibration data..." << std::endl;
            break;
        case AtlasScientificI2C::SETTEMPCOMPREAD:
        case AtlasScientificI2C::GETTEMPCOMP:
            std::cout << "pH ";
            decodeTempCompensation("pH", response);
            std::cout << std::endl;
            break;
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
            decodeInfoResponse("DO", response);
            std::cout << std::endl;
            break;
        case AtlasScientificI2C::STATUS:
            syslog(LOG_NOTICE, "got DO probe status event: %s\n", response.c_str());
            std::cout << "DO ";
            decodeStatusResponse("DO", response);
            std::cout << std::endl;
            break;
        case AtlasScientificI2C::CALIBRATE:
            if (response.find(",0") != std::string::npos)
                std::cout << "DO probe reports no calibration data..." << std::endl;
            break;
        case AtlasScientificI2C::SETTEMPCOMPREAD:
        case AtlasScientificI2C::GETTEMPCOMP:
            std::cout << "DO ";
            decodeTempCompensation("DO", response);
            std::cout << std::endl;
            break;
        default:
            break;
    }
}

void flowRateCallback(GpioInterrupt::MetaData *md)
{
    
}

void getWaterLevel()
{
}

void sendResultData()
{
    mqtt::message_ptr pubmsg;
    nlohmann::json j;
    unsigned int result = 0;
    std::time_t t = std::time(nullptr);
    char timebuff[100];

    memset(timebuff, '\0', 100);
    std::strftime(timebuff, 100, "%c", std::localtime(&t));

    j["aquarium"]["time"]["epoch"] = t;
    j["aquarium"]["time"]["local"] = timebuff;
    j["aquarium"]["waterlevel"] = Configuration::instance()->m_adc->reading(Configuration::instance()->m_adcWaterLevelIndex);
    
    if (Configuration::instance()->m_temp->enabled()) {
        std::map<std::string, std::string> devices = Configuration::instance()->m_temp->devices();
        auto it = devices.begin();
        while (it != devices.end()) {
            double c = Configuration::instance()->m_temp->getTemperatureByDevice(it->first);
            j["aquarium"]["temperature"][it->first]["celsius"] = c;
            j["aquarium"]["temperature"][it->first]["farenheit"] = Configuration::instance()->m_temp->convertToFarenheit(c);
            it++;
        }
    }

    if (Configuration::instance()->m_frEnabled) {
        j["aquarium"]["flowrate"]["gpm"] = Configuration::instance()->m_fr->gpm();
        j["aquarium"]["flowrate"]["lpm"] = Configuration::instance()->m_fr->lpm();
        j["aquarium"]["flowrate"]["hertz"] = Configuration::instance()->m_fr->hertz();
    }
    j["aquarium"]["ph"] = Configuration::instance()->m_ph->getPH();
    j["aquarium"]["oxygen"] = Configuration::instance()->m_oxygen->getDO();
    
    pubmsg = mqtt::make_message("aquarium/data", j.dump());
    if (Configuration::instance()->m_mqttConnected)
        Configuration::instance()->m_mqtt->publish(pubmsg);
}

void setTempCompensation()
{
    std::map<std::string, std::string> devices = Configuration::instance()->m_temp->devices();
    double c;

    auto it = devices.begin();

    if (it != devices.end()) {
        c = Configuration::instance()->m_temp->getTemperatureByDevice(it->first);

        std::cout << "Setting temp compensation value for probes to " << c << std::endl;
        if (c != 0) {
            Configuration::instance()->m_ph->setTempCompensation(c);
            Configuration::instance()->m_oxygen->setTempCompensation(c);
            Configuration::instance()->m_ph->getTempCompensation();
            Configuration::instance()->m_oxygen->getTempCompensation();
        }
        it++;
    }
}

void sendTempProbeIdentification()
{
    std::map<std::string, std::string> devices = Configuration::instance()->m_temp->devices();
    mqtt::message_ptr pubmsg;
    nlohmann::json j;

    auto it = devices.begin();
    
    while (it != devices.end()) {
        j["aquarium"]["device"]["ds18b20"]["name"] = it->second;
        j["aquarium"]["device"]["ds18b20"]["device"] = it->first;
        it++;
    }
    pubmsg = mqtt::make_message("aquarium/devices", j.dump());
    if (Configuration::instance()->m_mqttConnected)
        Configuration::instance()->m_mqtt->publish(pubmsg);
}

/*
 * { "aquarium":{"device":[{"serial":"name"},{"serial":"name"}]}}
 */
void nameTempProbe(std::string json)
{
    std::map<std::string, std::string> entry;
    auto j = nlohmann::json::parse(json);
    auto device = j.find("device");
    
    if (device != j.end()) {
        for (auto it : device->items()) {
            std::stringstream key;
            std::stringstream value;
            key << it.key();
            value << it.value();
            entry[key.str()] = value.str();
        }
        Configuration::instance()->addArray(std::string("ds18b20"), entry);
    }
}

void mqttIncomingMessage(std::string topic, std::string message)
{
    if (topic == "aquarium/ds18b20/name") {
        nameTempProbe(message);
    }
}

void mqttConnectionLost()
{
}

void mqttConnected()
{
    std::cout << "MQTT connected!" << std::endl;
    Configuration::instance()->m_mqttConnected = true;
    g_mqttCV.notify_one();
}

void mainloop()
{
    ITimer doUpdate;
    ITimer phUpdate;
    ITimer sendUpdate;
    ITimer tempCompensation;
    
    auto phfunc = []() { Configuration::instance()->m_ph->sendReadCommand(900); };
    auto dofunc = []() { Configuration::instance()->m_oxygen->sendReadCommand(600); };
    auto updateFunc = []() { sendResultData(); };
    auto compFunc = []() { setTempCompensation(); };
    
    doUpdate.setInterval(dofunc, TEN_SECONDS);
    phUpdate.setInterval(phfunc, TEN_SECONDS);
    sendUpdate.setInterval(updateFunc, ONE_MINUTE);
    tempCompensation.setInterval(compFunc, ONE_HOUR);
    
    setTempCompensation();
    
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    doUpdate.stop();
    phUpdate.stop();
    sendUpdate.stop();
    tempCompensation.stop();
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
bool parse_args(int argc, char **argv)
{
    int opt;
    bool rval = true;
    std::string cf = "~/.config/aquarium.conf";
    
    Configuration::instance()->m_daemonize = false;
    
    if (argv) {
        while ((opt = getopt(argc, argv, "c:hd")) != -1) {
            switch (opt) {
            case 'h':
                usage(argv[0]);
                return false;
                break;
            case 'c':
            cf = optarg;
                break;
            case 'd':
                Configuration::instance()->m_daemonize = true;
                break;
            default:
                syslog(LOG_ERR, "Unexpected command line argument given");
                usage(argv[0]);
                return false;
            }
        }
    }

    if (cf.size() > 0) {
        if ((cf.find("$HOME") != std::string::npos) || (cf.at(0) == '~')) {
            const char* homeDir = getenv("HOME");
            if (cf.at(0) == '~') {
                cf.erase(0, 1);
                cf.insert(0, homeDir);
            }
            if (cf.find("$HOME") == 0) {
                cf.erase(0, 5);
                cf.insert(0, homeDir);
            }
            std::cerr << __FUNCTION__ << ": Changing config file path to " << cf << std::endl;
            Configuration::instance()->setConfigFile(cf);
        }
    }

    return rval;
}

int main(int argc, char *argv[])
{
    std::string progname = basename(argv[0]);
    
    openlog(progname.c_str(), LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    
    syslog(LOG_NOTICE, "Application startup");
        
    if (!parse_args(argc, argv)) {
        std::cerr << "Error parsing command line, exiting..." << std::endl;
        syslog(LOG_ERR, "Error parsing command line, exiting...");
        exit(-1);
    }
    
    // if this goes badly, just die but leave the error LED blinking at 1 hz
    if (!Configuration::instance()->readConfigFile()) {
        std::cerr << "Unable to read configuration file, exiting..." << std::endl;
        syslog(LOG_ERR, "Unable to read configuration file, exiting...");
        exit(-2);
    }

    if (!GpioInterrupt::instance()->addPin(Configuration::instance()->m_greenLed, GpioInterrupt::GPIO_DIRECTION_OUT, GpioInterrupt::GPIO_IRQ_NONE))
        syslog(LOG_ERR, "Unable to open GPIO for green led");
    
    if (!GpioInterrupt::instance()->addPin(Configuration::instance()->m_yellowLed, GpioInterrupt::GPIO_DIRECTION_OUT, GpioInterrupt::GPIO_IRQ_NONE))
        syslog(LOG_ERR, "Unable to open GPIO for yellow led");

    if (!GpioInterrupt::instance()->addPin(Configuration::instance()->m_redLed, GpioInterrupt::GPIO_DIRECTION_OUT, GpioInterrupt::GPIO_IRQ_NONE))
        syslog(LOG_ERR, "Unable to open GPIO for red led");

    initializeLeds();

    mqtt::connect_options connopts;
    connopts.set_keep_alive_interval(20);
	connopts.set_clean_session(true);
    connopts.set_automatic_reconnect(1, 10);

    mqtt::async_client mqtt(Configuration::instance()->m_mqttServer, Configuration::instance()->m_localId);
    LocalMQTTCallback callback(mqtt, connopts);
    callback.setMessageCallback(mqttIncomingMessage);
    callback.setConnectionLostCallback(mqttConnectionLost);
    callback.setConnectedCallback(mqttConnected);
    mqtt.set_callback(callback);
    Configuration::instance()->m_mqtt = &mqtt;

    try {
		std::cout << "Connecting to the MQTT server..." << std::flush;
		mqtt.connect(connopts, nullptr, callback);
	}
	catch (const mqtt::exception&) {
		std::cerr << "\nERROR: Unable to connect to MQTT server: '" << Configuration::instance()->m_mqttServer << "'" << std::endl;
		exit(-3);
	}
    std::unique_lock<std::mutex> lk(g_mqttMutex);
    g_mqttCV.wait(lk, []{return true;});

    if (Configuration::instance()->m_frEnabled) {
        GpioInterrupt::instance()->addPin(Configuration::instance()->m_flowRatePin);
        GpioInterrupt::instance()->setPinCallback(Configuration::instance()->m_flowRatePin, flowRateCallback);
    }
    
    if (Configuration::instance()->m_aioEnabled)
        Configuration::instance()->m_aio = new AdafruitIO(Configuration::instance()->m_localId, 
                                                          Configuration::instance()->m_aioServer, 
                                                          Configuration::instance()->m_aioUserName, 
                                                          Configuration::instance()->m_aioKey, 
                                                          Configuration::instance()->m_aioPort);
    
    GpioInterrupt::instance()->start();
    Configuration::instance()->m_oxygen->setCallback(doCallback);
    Configuration::instance()->m_ph->setCallback(phCallback);

    Configuration::instance()->m_oxygen->sendInfoCommand();
    Configuration::instance()->m_oxygen->calibrate(DissolvedOxygen::QUERY, nullptr, 0);
    Configuration::instance()->m_oxygen->getTempCompensation();
    Configuration::instance()->m_oxygen->sendStatusCommand();

    Configuration::instance()->m_ph->sendInfoCommand();
    Configuration::instance()->m_ph->calibrate(PotentialHydrogen::QUERY, nullptr, 0);
    Configuration::instance()->m_ph->getTempCompensation();
    Configuration::instance()->m_ph->sendStatusCommand();
    
    sendTempProbeIdentification();
    
    mainloop();
    
    return 0;
}
