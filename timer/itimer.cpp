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

#include "itimer.h"

ITimer::ITimer()
{
    this->clear = false;
}

ITimer::~ITimer()
{
    this->clear = false;
}

void ITimer::setTimeout(std::function<void()> function, int interval)
{
    struct timespec st;
    st.tv_sec = interval / 1000;
    st.tv_nsec = (interval % 1000) * 1000000;

    std::cout << __FUNCTION__ << ": interval time of " << interval << std::endl;
    std::cout << __FUNCTION__ << ": Sleeping for " << st.tv_sec << "." << st.tv_nsec << std::endl;
    
    clear = false;
    std::thread t([=]() {
        if (nanosleep(&st, nullptr) < 0) {
            syslog(LOG_ERR, "nanosleep returned error: %s(%d)", strerror(errno), errno);
            return;
        }
        
        if (clear) {
            return;
        }
        
        try {
            function();
        }
        catch (std::exception &e) {
            syslog(LOG_ERR, "Unable to execute function: %s\n", e.what());
        }
    });
    t.detach();
}

void ITimer::setInterval(std::function<void()> function, int interval)
{
    struct timespec st;
    st.tv_sec = interval / 1000;
    st.tv_nsec = (interval % 1000) * 1000000;

    std::cout << __FUNCTION__ << ": interval time of " << interval << std::endl;
    std::cout << __FUNCTION__ << ": Sleeping for " << st.tv_sec << "." << st.tv_nsec << std::endl;

    this->clear = false;
    std::thread t([=]() {
        while(true) {
            if (nanosleep(&st, nullptr) < 0) {
                syslog(LOG_ERR, "nanosleep returned error: %s(%d)", strerror(errno), errno);
                return;
            }
            if(this->clear) 
                return;

            function();

            if(this->clear) 
                return;
        }
    });
    t.detach();
}
