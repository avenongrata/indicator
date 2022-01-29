#ifndef INDICATOR_DRIVER_H
#define INDICATOR_DRIVER_H

#define BASE_ADDR 0x40000000U
#define INDICATOR_OFFSET 0x00U
#define BITMASK_REGISTERS 0x3    /* bitmask of available registers */
#define REGISTER_COUNT 1
#define DRIVER_NAME "indicator_driver"
#define DRIVER_NAME_LEN 128

#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif

#endif /* INDICATOR_DRIVER_H */
