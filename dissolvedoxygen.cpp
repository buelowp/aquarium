/*
 * Copyright (c) 2019 <copyright holder> <email>
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

#include "dissolvedoxygen.h"

DissolvedOxygen::DissolvedOxygen(uint8_t device, uint8_t address) : AtlasScientificI2C(device, address)
{
}

DissolvedOxygen::~DissolvedOxygen()
{
}

void DissolvedOxygen::response(int cmd, uint8_t *buffer, int size)
{
    std::string r;
    
    fprintf(stderr, "got a response for cmd %d of size %d\n", cmd, size);
    for (int i = 0; i < size; i++) {
        r += static_cast<char>(buffer[i]);
    }
    
    try {
        m_callback(cmd, r);
    }
    catch (const std::bad_function_call& e) {
        syslog(LOG_ERR, "Caught exception: %s", e.what());
    }
}

void DissolvedOxygen::getLastResponse(std::string &r)
{
    r.clear();
    
    for (int i = 0; i < m_lastResponse.size(); i++) {
        r += static_cast<char>(m_lastResponse[i]);
    }
}
