#include "functions.h"

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
