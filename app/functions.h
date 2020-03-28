#ifndef __AQ_FUNCTIONS_H__
#define __AQ_FUNCTIONS_H__

#include <string>
#include "configuration.h"

void eternalBlinkAndDie(int pin, int millihz);
void initializeLeds();
bool cisCompare(const std::string & str1, const std::string &str2);

#endif
