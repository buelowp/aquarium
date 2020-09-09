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
#include <signal.h>
#include <libgen.h>
#include <errno.h>
#include <nlohmann/json.hpp>

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
std::mutex g_decodeMutex;
std::mutex g_statusMutex;
bool g_finished;
bool g_stopRapidFireWaterLevel;
bool g_exitImmediately;
int g_gpioPortOneState;
int g_gpioPortTwoState;

void gpioPortOneISR()
{
    g_gpioPortOneState = digitalRead(Configuration::instance()->m_gpioPortOne);
}

void gpioPortTwoISR()
{
    g_gpioPortTwoState = digitalRead(Configuration::instance()->m_gpioPortTwo);
}

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
    pinMode(Configuration::instance()->m_greenLed, OUTPUT);
    pinMode(Configuration::instance()->m_yellowLed, OUTPUT);
    pinMode(Configuration::instance()->m_redLed, OUTPUT);
    
    digitalWrite(Configuration::instance()->m_greenLed, HIGH);
    digitalWrite(Configuration::instance()->m_yellowLed, LOW);
    digitalWrite(Configuration::instance()->m_redLed, LOW);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    digitalWrite(Configuration::instance()->m_greenLed, LOW);
    digitalWrite(Configuration::instance()->m_yellowLed, HIGH);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    digitalWrite(Configuration::instance()->m_yellowLed, LOW);
    digitalWrite(Configuration::instance()->m_redLed, HIGH);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    digitalWrite(Configuration::instance()->m_redLed, LOW);
    digitalWrite(Configuration::instance()->m_greenLed, HIGH);
}

bool cisCompare(const std::string & str1, const std::string &str2)
{
    return ((str1.size() == str2.size()) && std::equal(str1.begin(), str1.end(), str2.begin(), 
            [](const char c1, const char c2)
            { return (c1 == c2 || std::toupper(c1) == std::toupper(c2)); }
            ));
}

void decodeStatusResponse(std::string which, std::string &response)
{
    static int lastWarningHandle = 0;
    static int lastErrorHandle = 0;
    nlohmann::json j;
    std::string::size_type pos = response.find_last_of(",");
    double voltage;
    
    g_statusMutex.lock();
    if (pos != std::string::npos) {
        if (lastErrorHandle > 0) {
            g_errors.clearCritical(lastErrorHandle);
            lastErrorHandle = 0;
        }
        voltage = std::stod(response.substr(pos + 1));
        if (voltage > 3) {
            if (lastWarningHandle > 0) {
                g_errors.clearWarning(lastWarningHandle);
                lastWarningHandle = 0;
            }
        }
        else {
            std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": probe is reporting an unusual voltage (" << voltage << "), it may not be operating correctly." << std::endl;
            lastWarningHandle = g_errors.warning(std::string(which + " probe is reporting undervoltage"), Configuration::instance()->m_mqtt, 0);
        }
        if (which == "pH") {
            j["aquarium"]["device"]["ph"]["voltage"] = response.substr(pos + 1);
            Configuration::instance()->m_phVoltage = response.substr(pos + 1);
        }
        else if (which == "DO") {
            j["aquarium"]["device"]["dissolvedoxygen"]["version"] = response.substr(pos + 1);
            Configuration::instance()->m_o2Voltage = response.substr(pos + 1);
        }
        if (Configuration::instance()->m_mqttConnected)
            Configuration::instance()->m_mqtt->publish("aquarium2/device", j.dump());
        
        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": " << which << ": probe is operating normally, reporting voltage " << voltage << std::endl;
    }
    else {
        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": probe status cannot be decoded" << std::endl;
        lastErrorHandle = g_errors.critical(std::string(which + " probe is returning garbage"), Configuration::instance()->m_mqtt, 0);
    }
    g_statusMutex.unlock();
}

void decodeInfoResponse(std::string which, std::string &response)
{
    static int lastErrorHandle = 0;
    nlohmann::json j;
    std::string::size_type pos = response.find_last_of(",");
    
    if (pos != std::string::npos) {
        if (lastErrorHandle > 0) {
            g_errors.clearCritical(lastErrorHandle);
            lastErrorHandle = 0;
        }
        if (which == "pH") {
            j["aquarium"]["device"]["ph"]["version"] = response.substr(pos + 1);
            Configuration::instance()->m_phVersion = response.substr(pos + 1);
        }
        else if (which == "DO") {
            j["aquarium"]["device"]["dissolvedoxygen"]["version"] = response.substr(pos + 1);
            Configuration::instance()->m_o2Version = response.substr(pos + 1);
        }

        if (Configuration::instance()->m_mqttConnected)
            Configuration::instance()->m_mqtt->publish("aquarium2/device", j.dump());
        
        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": " << which << ": version " << response.substr(pos + 1) << std::endl;        
    }
    else {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": probe info response cannot be decoded" << std::endl;
        lastErrorHandle = g_errors.critical(std::string(which + " probe is returning garbage"), Configuration::instance()->m_mqtt, 0);
    }
}

void decodeTempCompensation(std::string which, std::string &response)
{
    static int lastErrorHandle = 0;
    nlohmann::json j;

    std::string::size_type pos = response.find_last_of(",");
    if (pos != std::string::npos) {
        if (which == "pH") {
            j["aquarium"]["device"]["ph"]["tempcompensation"] = response.substr(pos + 1);
            Configuration::instance()->m_o2TempComp = response.substr(pos + 1);
        }
        else if (which == "DO") {
            j["aquarium"]["device"]["dissolvedoxygen"]["tempcompensation"] = response.substr(pos + 1);
            Configuration::instance()->m_o2TempComp = response.substr(pos + 1);
        }
        Configuration::instance()->m_mqtt->publish("aquarium2/device", j.dump());
        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": " << which << ": probe has a temp compensation value of " << response.substr(pos + 1) << "C" << std::endl;
    }
    else {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": probe temp compensation response cannot be decoded" << std::endl;
        lastErrorHandle = g_errors.critical(std::string(which + " probe is returning garbage"), Configuration::instance()->m_mqtt, 0);
    }
}

void phCallback(int cmd, std::string response)
{
    switch (cmd) {
        case AtlasScientificI2C::INFO:
            syslog(LOG_NOTICE, "got pH probe info event: %s\n", response.c_str());
            decodeInfoResponse("pH", response);
            break;
        case AtlasScientificI2C::STATUS:
            syslog(LOG_NOTICE, "got pH probe status event: %s\n", response.c_str());
            decodeStatusResponse("pH", response);
            break;
        case AtlasScientificI2C::CALIBRATE:
            if (response.find(",0") != std::string::npos)
                std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": pH: probe reports no calibration data..." << std::endl;
            break;
        case AtlasScientificI2C::SETTEMPCOMPREAD:
        case AtlasScientificI2C::GETTEMPCOMP:
            decodeTempCompensation("pH", response);
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
            decodeInfoResponse("DO", response);
            break;
        case AtlasScientificI2C::STATUS:
            syslog(LOG_NOTICE, "got DO probe status event: %s\n", response.c_str());
            decodeStatusResponse("DO", response);
            break;
        case AtlasScientificI2C::CALIBRATE:
            if (response.find(",0") != std::string::npos)
                std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": DO: probe reports no calibration data..." << std::endl;
            break;
        case AtlasScientificI2C::SETTEMPCOMPREAD:
        case AtlasScientificI2C::GETTEMPCOMP:
            decodeTempCompensation("DO", response);
            break;
        default:
            break;
    }
}

void sendLocalResultData()
{
    nlohmann::json j;
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
    if (Configuration::instance()->m_gpioPortOne != 0) {
        j["aquarium"]["gpio"]["1"] = g_gpioPortOneState;
    }
    
    if (Configuration::instance()->m_gpioPortTwo != 0) {
        j["aquarium"]["gpio"]["2"] = g_gpioPortTwoState;
    }
    
    if (Configuration::instance()->m_mqttConnected)
        Configuration::instance()->m_mqtt->publish("aquarium2/data", j.dump());
}

void sendAIOResultData()
{
    if (Configuration::instance()->m_aioConnected) {
        nlohmann::json wlj;
        nlohmann::json o2j;
        nlohmann::json phj;
        nlohmann::json tempj;
        
        wlj["value"] = Configuration::instance()->m_adc->reading(Configuration::instance()->m_adcWaterLevelIndex);
        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ <<  ": pbuelow/feeds/aquarium.waterlevel: " << wlj.dump() << std::endl;
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

        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Setting temp compensation value for probes to " << c << std::endl;
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
    nlohmann::json j;
    int index = 1;

    auto it = devices.begin();
    
    while (it != devices.end()) {
        j["aquarium"]["device"]["ds18b20"][index]["name"] = it->second;
        j["aquarium"]["device"]["ds18b20"][index]["device"] = it->first;
        it++;
        index++;
    }
    if (Configuration::instance()->m_mqtt->is_connected())
        Configuration::instance()->m_mqtt->publish("aquarium2/devices", j.dump());
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
            std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ <<  ": " << el.key() << ":" << el.value() << std::endl;
            entry[el.key()] = el.value();
        }
    }
    else {
        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ <<  ": j is null: " << json << std::endl;
    }
    
    if (entry.size())
        Configuration::instance()->updateArray(std::string("ds18b20"), entry);
}

void rapidFireWaterLevelMessaging(void *t)
{
    nlohmann::json j;
    ITimer *timer = static_cast<ITimer*>(t);
    
    if (g_stopRapidFireWaterLevel) {
        timer->stop();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        delete timer;
        return;
    }
    j["aquarium"]["waterlevel"] = Configuration::instance()->m_adc->reading(Configuration::instance()->m_adcWaterLevelIndex);

    if (Configuration::instance()->m_mqtt->is_connected())
        Configuration::instance()->m_mqtt->publish("aquarium2/waterlevel/value", j.dump());
}

void mqttIncomingMessage(std::string topic, std::string message)
{
    std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ <<  ": Handling topic " << topic << std::endl;
    if (topic == "aquarium2/set/ds18b20") {
        nameTempProbe(message);
    }
    if (topic == "aquarium2/waterlevel/rapidfire/start") {
        ITimer *t = new ITimer();
        t->setInterval(rapidFireWaterLevelMessaging, 500);
        g_stopRapidFireWaterLevel = false;
    }
    if (topic == "aquarium2/waterlevel/rapidfire/stop") {
        g_stopRapidFireWaterLevel = true;        
    }
}

void mqttConnectionLost(const std::string &cause)
{
    std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << "MQTT disconnected: " << cause << std::endl;
    Configuration::instance()->m_mqttConnected = false;
    g_errors.warning("MQTT connection lost", Configuration::instance()->m_mqtt, 0, ErrorHandler::StaticErrorHandles::MqttConnectionLost);
}

void mqttConnected()
{
    std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": MQTT connected!" << std::endl;
    Configuration::instance()->m_mqttConnected = true;

    Configuration::instance()->m_mqtt->subscribe("aquarium2/set/#", 1);
    Configuration::instance()->m_mqtt->subscribe("aquarium2/waterlevel/rapidfire/#", 1);

    g_finished = true;
    g_mqttCV.notify_all();
    g_errors.clearWarning(ErrorHandler::StaticErrorHandles::MqttConnectionLost);
}

void aioIncomingMessage(std::string topic, std::string message)
{
    std::cout << __FUNCTION__ << ": Odd, we shouldn't get messages from AIO" << std::endl;
}

void aioConnected()
{
    std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << "AIO connected!" << std::endl;
    Configuration::instance()->m_aioConnected = true;
}

void aioConnectionLost(const std::string &cause)
{
    std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__  << ": AIO disconnected: " << cause << std::endl;
    Configuration::instance()->m_aioEnabled = false;
}

/*
 * \fn void mainloop()
 * 
 * The while(1) loop. This keeps the entire system alive while the async
 * bits of code function in their own threads.
 */
void mainloop()
{
    ITimer doUpdate;
    ITimer phUpdate;
    ITimer sendLocalUpdate;
    ITimer tempCompensation;
    ITimer sendAIOUpdate;
    
    auto phfunc = [](void*) { Configuration::instance()->m_ph->sendReadCommand(900); };
    auto dofunc = [](void*) { Configuration::instance()->m_oxygen->sendReadCommand(600); };
    auto updateLocalFunc = [](void*) { sendLocalResultData(); };
    auto compFunc = [](void*) { setTempCompensation(); };
    auto updateAIO = [](void*) { sendAIOResultData(); };
    
    doUpdate.setInterval(dofunc, TEN_SECONDS);
    phUpdate.setInterval(phfunc, TEN_SECONDS);
    sendLocalUpdate.setInterval(updateLocalFunc, ONE_MINUTE);
    sendAIOUpdate.setInterval(updateAIO, ONE_MINUTE);
    tempCompensation.setInterval(compFunc, ONE_HOUR);
    
    setTempCompensation();
    
    while (!g_exitImmediately) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": exiting main loop..." << std::endl;
    
    auto toks = Configuration::instance()->m_mqtt->get_pending_delivery_tokens();
    if (!toks.empty())
        std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Error: There are pending delivery tokens!" << std::endl;

    std::cout << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Disconnecting MQTT" << std::endl;
    auto conntok = Configuration::instance()->m_mqtt->disconnect();
    conntok->wait();
    
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
    std::cerr << "\t-d Daemonize the application to run in the background (currently not functional)" << std::endl;
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
            std::cerr  << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": Changing config file path to " << cf << std::endl;
            Configuration::instance()->setConfigFile(cf);
        }
    }

    return rval;
}

void handle_sigint(int sig)
{
    g_exitImmediately = true;
    std::cerr << "Exiting due to signal";
    syslog(LOG_ERR, "Exiting due to signal %d", sig);
    digitalWrite(Configuration::instance()->m_greenLed, LOW);
    digitalWrite(Configuration::instance()->m_yellowLed, LOW);
    digitalWrite(Configuration::instance()->m_redLed, HIGH);
}

/**
 * \fn int main(int argc, char *argv[])
 * 
 * Add more documentation here
 */
int main(int argc, char *argv[])
{
    std::string progname = basename(argv[0]);
    g_finished = false;
    g_exitImmediately = false;
    
    openlog(progname.c_str(), LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    
    syslog(LOG_NOTICE, "Application startup");
    
    /** Do our best to clean up and exit if we can **/
    signal(SIGINT, handle_sigint);
    signal(SIGILL, handle_sigint);
    signal(SIGABRT, handle_sigint);
    signal(SIGFPE, handle_sigint);
    
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

    std::unique_lock<std::mutex> lk(g_mqttMutex);
    Configuration::instance()->createLocalConnection();
    Configuration::instance()->createAIOConnection();

    g_mqttCV.wait(lk, []{return g_finished;});

    Configuration::instance()->m_mqtt->start_consuming();

    Configuration::instance()->m_oxygen->setCallback(doCallback);
    Configuration::instance()->m_ph->setCallback(phCallback);

    Configuration::instance()->m_oxygen->sendInfoCommand();
    Configuration::instance()->m_oxygen->calibrate(DissolvedOxygen::DO_QUERY, nullptr, 0);
    Configuration::instance()->m_oxygen->getTempCompensation();
    Configuration::instance()->m_oxygen->sendStatusCommand();
    Configuration::instance()->m_oxygen->disableLeds();


    Configuration::instance()->m_ph->sendInfoCommand();
    Configuration::instance()->m_ph->calibrate(PotentialHydrogen::PH_QUERY, nullptr, 0);
    Configuration::instance()->m_ph->getTempCompensation();
    Configuration::instance()->m_ph->sendStatusCommand();
    Configuration::instance()->m_ph->disableLeds();
    
    if (Configuration::instance()->m_gpioPortOne != 0) {
        wiringPiISR(Configuration::instance()->m_gpioPortOne, INT_EDGE_BOTH, gpioPortOneISR);
    }
    
    if (Configuration::instance()->m_gpioPortTwo != 0) {
        wiringPiISR(Configuration::instance()->m_gpioPortTwo, INT_EDGE_BOTH, gpioPortTwoISR);
    }

    sendTempProbeIdentification();
    
    mainloop();
    
    return 0;
}
