#include "cindicator.h"

namespace drv
{

//=============================================================================

CIndicator::CIndicator() : CDriverRegion{s_regs} {}
CIndicator::~CIndicator() {}

//=============================================================================

// Запросить цвет.
IIndicator::color_t CIndicator::getColor()
{
    lock();
    auto value = lReg<color_t>(0x00U);
    unlock();

    return value;
}

//-----------------------------------------------------------------------------

// Задать цвет.
void CIndicator::setColor(const color_t & type)
{
    lock();
    lReg<color_t>(0x00U) = type;
    unlock();
}

//=============================================================================

} // namespace drv
