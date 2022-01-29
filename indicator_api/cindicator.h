#ifndef DRV_CINDICATOR_H
#define DRV_CINDICATOR_H

#include "iindicator.h"
#include "device-library/cdrvreg.h"

namespace drv
{

using namespace api::dev;
using namespace dev::drv;

//=============================================================================

class CIndicator : public CDriverRegion, public IIndicator
{
public:
    CIndicator();
    ~CIndicator() override;

    //-------------------------------------------------------------------------

    // Запросить цвет.
    color_t getColor() override;

    //-------------------------------------------------------------------------

    // Задать цвет.
    void setColor(const color_t & type) override;

    //-------------------------------------------------------------------------

    // Запросить интерфейс региона.
    IDriverRegion & region() override { return CDriverRegion::region(); }

private:
    //-------------------------------------------------------------------------

    // Количество регистров.
    constexpr static const size_t s_regs = 0x01U;
};

//=============================================================================

} // namespace drv

#endif // DRV_CINDICATOR_H
