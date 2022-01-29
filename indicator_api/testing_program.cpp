#include <iostream>
#include <string>
#include "cindicator.h"
#include "device-library/cdevsym.h"

#include <thread>
#include <chrono>

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>


template<typename PT, typename VT>
void printValue(const std::string & title, const VT & value)
{
    std::cout << title << " -> " << PT(value) << std::endl;
}

void print(const std::string & title, const bool & value)
{
    std::cout << title << " -> " << ((value) ? "yes" : "no") << std::endl;
}

void print(const std::string & title, const uint8_t & value)
{ printValue<uint16_t>(title, value); }

void print(const std::string & title, const uint16_t & value)
{ printValue<uint16_t>(title, value); }

void print(const std::string & title, const uint32_t & value)
{ printValue<uint32_t>(title, value); }

void print(const std::string & text)
{ std::cout << text << std::endl; }

void readTest(drv::IIndicator * test)
{
    test->region().reset();
    if (!test->region().recv())
    {
        print("Can't get region");
        exit(EXIT_FAILURE);
    }
    else
    {
        print("Got region");
    }

    drv::CIndicator::color_t clr;
    clr = test->getColor();
    switch (clr)
    {
        case drv::CIndicator::color_t::OFF: print("OFF"); break;
        case drv::CIndicator::color_t::RED: print("RED"); break;
        case drv::CIndicator::color_t::GREEN: print("GREEN"); break;
        case drv::CIndicator::color_t::GREEN_RED: print("GREEN_RED"); break;
        case drv::CIndicator::color_t::BLUE: print("BLUE"); break;
        case drv::CIndicator::color_t::BLUE_RED: print("BLUE_RED"); break;
        case drv::CIndicator::color_t::TURQUOISE: print("TURQUOISE"); break;
        case drv::CIndicator::color_t::TURQUOISE_RED: print("TURQUOISE_RED"); break;
    }
    print("###############################################\n");
}

void updateRegion(drv::IIndicator * test, const std::string & msg)
{
    test->region().send();
    print("\n###############################################\n\n");
    print(msg + "\n");
    readTest(test);
}

void writeTest(drv::IIndicator * test)
{
    test->region().reset();
    test->region().send();

    drv::CIndicator::color_t clr;

//-----------------------------------------------------------------------------
    clr = drv::CIndicator::color_t::RED;
    test->setColor(clr);
    updateRegion(test, "RED");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
//-----------------------------------------------------------------------------
    clr = drv::CIndicator::color_t::GREEN;
    test->setColor(clr);
    updateRegion(test, "GREEN");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
//-----------------------------------------------------------------------------
    clr = drv::CIndicator::color_t::GREEN_RED;
    test->setColor(clr);
    updateRegion(test, "GREEN_RED");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
//-----------------------------------------------------------------------------
    clr = drv::CIndicator::color_t::BLUE;
    test->setColor(clr);
    updateRegion(test, "BLUE");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
//-----------------------------------------------------------------------------
    clr = drv::CIndicator::color_t::BLUE_RED;
    test->setColor(clr);
    updateRegion(test, "BLUE_RED");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
//-----------------------------------------------------------------------------
    clr = drv::CIndicator::color_t::TURQUOISE;
    test->setColor(clr);
    updateRegion(test, "TURQUOISE");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
//-----------------------------------------------------------------------------
    clr = drv::CIndicator::color_t::TURQUOISE_RED;
    test->setColor(clr);
    updateRegion(test, "TURQUOISE_RED");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

int main()
{
    auto dev = new dev::sym::CDevSym();
    auto ind = new drv::CIndicator();
    dev::sym::IDevSym * sym = dev;

    sym->setDevPath("/dev/indicator_driver_40000000");
    sym->setMaxSize(ind->region().getSize());

    if (!sym->devOpen())
    {
        exit(EXIT_FAILURE);
    }
    else
    {
        print("File opened");
    }

    dev->setMemIO(ind->region().getIntMemIO());
    ind->region().setDevIO(dev->getIntDevIO());

    while (true)
    {
        writeTest(ind);
    }

    ind->region().reset();
    ind->region().send();

    dev->devClose();

    return 0;
}
