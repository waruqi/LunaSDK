/*!
* This file is a portion of Luna SDK.
* For conditions of distribution and use, see the disclaimer
* and license in LICENSE.txt
* 
* @file Main.cpp
* @author JXMaster
* @date 2023/8/27
*/
#include "TestCommon.hpp"
#include <Luna/Runtime/Runtime.hpp>
using namespace Luna;

int main()
{
    Luna::init();
    json_test();
    diff_test();
    Luna::close();
    return 0;
}