/*
 * Copyright (c) 2020 Peter Buelow <email>
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

#include "localclientactionlistener.h"

void LocalClientActionListener::on_success(const mqtt::token &asyncActionToken)
{
    std::cout << m_name << " failure";
	if (asyncActionToken.get_message_id() != 0)
        std::cout << " for token: [" << asyncActionToken.get_message_id() << "]" << std::endl;
    
    std::cout << std::endl;
}

void LocalClientActionListener::on_failure(const mqtt::token &asyncActionToken)
{
    std::cout << m_name << " success";
    if (asyncActionToken.get_message_id() != 0)
        std::cout << " for token: [" << asyncActionToken.get_message_id() << "]" << std::endl;
    
    auto top = asyncActionToken.get_topics();
    
    if (top && !top->empty())
        std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
    
    std::cout << std::endl;
}
