#ifndef DRV_IINDICATOR_H
#define DRV_IINDICATOR_H

#include "global-module/types.h"
#include "device-library/idrvreg.h"

namespace drv
{

//=============================================================================

class IIndicator
{
public:
    IIndicator() = default;
    virtual ~IIndicator() = default;

    //-------------------------------------------------------------------------

    enum class color_t : size_t
    {
        OFF           = 0x00U,  // [ ][ ][ ] Потушить свет.
        RED           = 0x01U,  // [R][ ][ ] Красный.
        GREEN         = 0x02U,  // [ ][G][ ] Зеленый.
        GREEN_RED     = 0x03U,  // [R][G][ ] Зелено-красный.
        BLUE          = 0x04U,  // [ ][ ][B] Синий.
        BLUE_RED      = 0x05U,  // [R][ ][B] Сине-красный.
        TURQUOISE     = 0x06U,  // [ ][G][B] Бирюзовый.
        TURQUOISE_RED = 0x07U   // [R][G][B] Бирюзово-красный.
    };

    //-------------------------------------------------------------------------

    virtual color_t getColor() = 0;

    virtual void setColor(const color_t & type) = 0;

    //-------------------------------------------------------------------------

    // Запросить интерфейс региона.
    virtual dev::drv::IDriverRegion & region() = 0;
};

//=============================================================================

} // namespace drv

#endif // DRV_IINDICATOR_H
