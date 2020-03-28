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

#include <wiringPi.h>
#include <syslog.h>
#include <libgen.h>
#include <errno.h>
#include <nlohmann/json.hpp>

#include "functions.h"
#include "potentialhydrogen.h"
#include "dissolvedoxygen.h"
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
bool g_finished;

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

void sendLocalResultData()
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
            j["aquarium"]["temperature"][it->second]["celsius"] = c;
            j["aquarium"]["temperature"][it->second]["farenheit"] = Configuration::instance()->m_temp->convertToFarenheit(c);
            it++;
        }
    }

    j["aquarium"]["ph"] = Configuration::instance()->m_ph->getPH();
    j["aquarium"]["oxygen"] = Configuration::instance()->m_oxygen->getDO();
    
    pubmsg = mqtt::make_message("aquarium/data", j.dump());
    if (Configuration::instance()->m_mqttConnected)
        Configuration::instance()->m_mqtt->publish(pubmsg);
}

void sendAIOResultData()
{
    if (Configuration::instance()->m_aioConnected) {
        nlohmann::json wlj;
        nlohmann::json o2j;
        nlohmann::json phj;
        nlohmann::json tempj;
        
        wlj["value"] = Configuration::instance()->m_adc->reading(Configuration::instance()->m_adcWaterLevelIndex);
        std::cout << __FUNCTION__ << ": pbuelow/feeds/aquarium.waterlevel: " << wlj.dump() << std::endl;
        mqtt::message_ptr wl = mqtt::make_message("pbuelow/feeds/aquarium.waterlevel", wlj.dump());
        Configuration::instance()->m_aio->publish(wl);
        
        phj["value"] = Configuration::instance()->m_ph->getPH();
        mqtt::message_ptr ph = mqtt::make_message("pbuelow/feeds/aquarium.ph", phj.dump());
        Configuration::instance()->m_aio->publish(ph);
        
        o2j["value"] = Configuration::instance()->m_oxygen->getDO();
        mqtt::message_ptr oxy = mqtt::make_message("pbuelow/feeds/aquarium.oxygen", o2j.dump());
        Configuration::instance()->m_aio->publish(oxy);

        if (Configuration::instance()->m_temp->enabled()) {
            std::map<std::string, std::string> devices = Configuration::instance()->m_temp->devices();
            auto it = devices.begin();
            if (it != devices.end()) {
                double c = Configuration::instance()->m_temp->getTemperatureByDevice(it->first);
                tempj["value"] = Configuration::instance()->m_temp->convertToFarenheit(c);
                mqtt::message_ptr temp = mqtt::make_message("pbuelow/feeds/aquarium.temperature", tempj.dump());
                Configuration::instance()->m_aio->publish(temp);
            }
        }
    }
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
    int index = 1;

    auto it = devices.begin();
    
    while (it != devices.end()) {
        j["aquarium"]["device"]["ds18b20"][index]["name"] = it->second;
        j["aquarium"]["device"]["ds18b20"][index]["device"] = it->first;
        it++;
        index++;
    }
    pubmsg = mqtt::make_message("aquarium/devices", j.dump());
    Configuration::instance()->m_mqtt->publish(pubmsg);
}

/*
 * {"serial":"name"}
 */
void nameTempProbe(std::string json)
{
    std::map<std::string, std::string> entry;
    auto j = nlohmann::json::parse(json);

    if (!j.is_null()) {
        for (auto& el : j.items()) {
            std::cout << __FUNCTION__ << ": " << el.key() << ":" << el.value() << std::endl;
            entry[el.key()] = el.value();
        }
    }
    else {
        std::cout << __FUNCTION__ << ": j is null: " << json << std::endl;
    }
    
    if (entry.size())
        Configuration::instance()->updateArray(std::string("ds18b20"), entry);
}

void mqttIncomingMessage(std::string topic, std::string message)
{
    std::cout << __FUNCTION__ << ": Handling topic " << topic << std::endl;
    if (topic == "aquarium/set/ds18b20") {
        nameTempProbe(message);
    }
}

void mqttConnectionLost(const std::string &cause)
{
    std::cout << "MQTT disconnected: " << cause << std::endl;
    Configuration::instance()->m_mqttConnected = false;
}

void mqttConnected()
{
    std::cout << "MQTT connected!" << std::endl;
    Configuration::instance()->m_mqttConnected = true;
    g_finished = true;
    g_mqttCV.notify_all();
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

void mainloop()
{
    ITimer doUpdate;
    ITimer phUpdate;
    ITimer sendLocalUpdate;
    ITimer tempCompensation;
    ITimer sendAIOUpdate;
    
    Configuration::instance()->m_mqtt->start_consuming();
    Configuration::instance()->m_mqtt->subscribe("aquarium/set/#", 1);

    auto phfunc = []() { Configuration::instance()->m_ph->sendReadCommand(900); };
    auto dofunc = []() { Configuration::instance()->m_oxygen->sendReadCommand(600); };
    auto updateLocalFunc = []() { sendLocalResultData(); };
    auto compFunc = []() { setTempCompensation(); };
    auto updateAIO = []() { sendAIOResultData(); };
    
    doUpdate.setInterval(dofunc, TEN_SECONDS);
    phUpdate.setInterval(phfunc, TEN_SECONDS);
    sendLocalUpdate.setInterval(updateLocalFunc, ONE_MINUTE);
    sendAIOUpdate.setInterval(updateAIO, ONE_MINUTE);
    tempCompensation.setInterval(compFunc, ONE_HOUR);
    
    setTempCompensation();
    
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    doUpdate.stop();
    phUpdate.stop();
    sendLocalUpdate.stop();
    sendAIOUpdate.stop();
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
    g_finished = false;
    
    g_finished = false;
    
    openlog(progname.c_str(), LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    
    syslog(LOG_NOTICE, "Application startup");
    
    wiringPiSetupGpio();
    piHiPri(99);
    
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

    initializeLeds();

    Configuration::instance()->createLocalConnection();
    Configuration::instance()->createAIOConnection();

    std::unique_lock<std::mutex> lk(g_mqttMutex);
    g_mqttCV.wait(lk, []{return g_finished;});

    Configuration::instance()->m_oxygen->setCallback(doCallback);
    Configuration::instance()->m_ph->setCallback(phCallback);

    Configuration::instance()->m_oxygen->sendInfoCommand();
    Configuration::instance()->m_oxygen->calibrate(DissolvedOxygen::DO_QUERY, nullptr, 0);
    Configuration::instance()->m_oxygen->getTempCompensation();
    Configuration::instance()->m_oxygen->sendStatusCommand();

    Configuration::instance()->m_ph->sendInfoCommand();
    Configuration::instance()->m_ph->calibrate(PotentialHydrogen::PH_QUERY, nullptr, 0);
    Configuration::instance()->m_ph->getTempCompensation();
    Configuration::instance()->m_ph->sendStatusCommand();
    
    sendTempProbeIdentification();
    
    mainloop();
    
    return 0;
}
