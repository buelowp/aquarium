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

#include "flowrate.h"

FlowRate::FlowRate()
{
    m_thread = new std::thread(&FlowRate::run, this);
    m_enabled = false;
    m_lpm = 0.0;
    m_gpm = 0.0;
}

FlowRate::~FlowRate()
{
}

FlowRate& FlowRate::operator++(int)
{
    if (m_enabled)
        m_count++;
}

void FlowRate::run()
{
    m_enabled = true;
    std::cout << __FUNCTION__ << ": Running thread, enabled = " << m_enabled << std::endl;
    while (m_enabled) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (m_count > 0) {
            m_hertz = m_count;
            m_count = 0;
            m_lpm = 0.3256 * m_hertz + 5.2004;
            m_gpm = m_lpm * .263;
        }
        else {
            m_lpm = 0.0;
            m_gpm = 0.0;
        }
    }
}

