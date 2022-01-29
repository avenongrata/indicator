/*
 * Xilinx Led-indicator driver
 *
 * Copyright (C) 2021 Kirill Yustitskii
 *
 * Authors:  Kirill Yustitskii  <inst: yustitskii_kirill>
 * 
 *           Alexey Aleksandrov 
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/fs.h>     
#include <linux/of.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>   
#include <linux/poll.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include "indicator_driver.h" 

//-----------------------------------------------------------------------------

// Локальный регион (массив слов).
struct local_region
{
    u32 reg[REGISTER_COUNT];    /* region of registers */    
    struct mutex access_mutex;  /* mutex for read/write operations */
};

//-----------------------------------------------------------------------------

// Параметры драйвера для взаимодействия с IP-Core.
struct ip_core
{
    struct resource * mem;       /* physical memory */
    void __iomem * io_base;      /* kernel space memory */
    struct local_region region; 
    
    struct device * dt_device;   /* device created form the device tree */
    struct device * device;      /* device associated with char_device */
    dev_t devt;                 /* our char device number */
    struct cdev char_device;    /* our char device */
};
//-----------------------------------------------------------------------------

/* queue for polling */
static DECLARE_WAIT_QUEUE_HEAD(indicator_read_wait);
static DECLARE_WAIT_QUEUE_HEAD(indicator_write_wait);

//-----------------------------------------------------------------------------

static struct class * ipcore_driver_class;   /* char device class */

//-----------------------------------------------------------------------------

// Получить адрес из смещения.
static void __iomem * get_address(struct ip_core * led, u32 offset)
{
    return led->io_base + offset;
}

//-----------------------------------------------------------------------------

// Прочитать регистры в локальный регион.
static void region_read(struct device * dev)
{   
    struct ip_core * led    = dev_get_drvdata(dev);
    void __iomem   * addr   = get_address(led, 0x00U);
    u32 mask    = BITMASK_REGISTERS;   /* bitmask of the available registers */
    int index   = 0;
    
    for (; mask != 0x00U; mask >>= 2)
    {
        if (mask & 0b01U)           /* available for read */
            led->region.reg[index++] = ioread32(addr); 
    
        addr += 4;
    }
}

//-----------------------------------------------------------------------------

// Записать регистры из локального региона.
static void region_write(struct device * dev)
{   
    struct ip_core * led   = dev_get_drvdata(dev);
    void __iomem   * addr  = get_address(led, 0x00U);
    u32 mask    = BITMASK_REGISTERS;   /* bitmask of the available registers */
    int index   = 0;
    
    for (; mask != 0x00U; mask >>= 2)
    {
        if (mask & 0b10U)          /* available for write */
            iowrite32(led->region.reg[index++], addr);
                
        addr += 4;
    }
}

//-----------------------------------------------------------------------------

// Определить битовое смещение по маске.
static u32 get_mask_rank(u32 mask)
{
    u32 count = 0;
    
    while (!((mask >> count) & 1))
            count++;
            
    return count;
}

//-----------------------------------------------------------------------------

// Применить параметр к машинному слову.
static u32 apply_parameter(void __iomem * addr, u32 mask, u32 value)
{
    u32 reg_read;

    reg_read = ioread32(addr);
    reg_read &= ~mask;                  /* reset to zero necessary bits */
    value <<= get_mask_rank(mask);      /* shift value by mask */
    reg_read |= value;                  /* set register with new value */

    return reg_read;
}

//-----------------------------------------------------------------------------

// Определить выход значения за ограничение маски.
static bool check_if_within_mask(u32 mask, u32 value)
{
    if (value > (mask >> get_mask_rank(mask)))
    {
        return false;
    }
    else 
    {
        return true;
    }
}

//-----------------------------------------------------------------------------

// Чтение для функций SysFS.
static ssize_t sysfs_read(struct device * dev, char * buf,
                        unsigned int addr_offset, u32 mask)
{
    u32 read_val, len;
    char tmp[32];
    void __iomem * addr;
    struct ip_core * led = dev_get_drvdata(dev);
    
    addr = get_address(led, addr_offset);
    read_val = ioread32(addr);
    read_val &= mask;                       /* get only necessary bits */
    read_val >>= get_mask_rank(mask);       /* make pretty print */
    len =  snprintf(tmp, sizeof(tmp), "0x%x\n", read_val);
    memcpy(buf, tmp, len);
    
    return len;
}

//-----------------------------------------------------------------------------

// Запись для функций SysFS.
static ssize_t sysfs_write(struct device * dev, const char * buf,
                        size_t count, unsigned int addr_offset,
                        u32 mask)
{
    struct ip_core * led   = dev_get_drvdata(dev);
    void __iomem   * addr  = get_address(led, addr_offset);
    unsigned long tmp;
    u32 value;
    int ret;
    
    ret = kstrtoul(buf, 0, &tmp);
    if (ret < 0)
        return ret;
    
    if (!check_if_within_mask(mask, (u32) tmp))
    {
        dev_warn(dev, "Invalid value {0x%lx} for address {0x%p}\n", 
                tmp, addr);
        return -EINVAL;
    }

    value = apply_parameter(addr, mask, (u32) tmp);
    iowrite32(value, addr);

    return count;
}

//-----------------------------------------------------------------------------
//  Функции символьного устройства.
//-----------------------------------------------------------------------------

// Запрос контекста для символьного устройства.
static int indicator_open(struct inode * inode, struct file * filp)
{
    struct ip_core * led = (struct ip_core *)container_of(inode->i_cdev,
                                                         struct ip_core,
                                                         char_device);
    filp->private_data = led;
    
    return 0;    
}

//-----------------------------------------------------------------------------

// Освобождение контекста символьного устройства.
static int indicator_release(struct inode * inode, struct file * filp)
{
    filp->private_data = NULL;
    
    return 0;
}

//-----------------------------------------------------------------------------

// Чтение в символьное устройство.
static ssize_t indicator_read(struct file * filp, char __user * buf,
                    size_t count, loff_t * pos)
{
    struct ip_core * led   = (struct ip_core *)filp->private_data;
    struct device  * dev   = led->device;
    size_t region_size     = sizeof(led->region.reg);

    if (count != region_size)
    {
        dev_err(dev, "incorrect read-value for region: %d "
            "instead of %d\n", count, region_size);
        return -EINVAL;
    }        
        
    wake_up_interruptible(&indicator_read_wait);

    mutex_lock(&led->region.access_mutex);
    region_read(dev);
    if (copy_to_user(buf, led->region.reg, region_size))
    {
        dev_err(dev, "can't copy local region to user\n");
        mutex_unlock(&led->region.access_mutex);
        return -EFAULT;
    }
    mutex_unlock(&led->region.access_mutex);
    
    return count;
}

//-----------------------------------------------------------------------------

// Запись в символьное устройство.
static ssize_t indicator_write(struct file * filp, const char __user * buf,
                    size_t count, loff_t * pos)
{
    struct ip_core * led   = (struct ip_core *)filp->private_data;
    struct device  * dev   = led->device;
    size_t region_size     = sizeof(led->region.reg);
    
    if (count != region_size)
    {
        dev_err(dev, "incorrect write-value for region: %d "
            "instead of %d\n", count, region_size);
        return -EINVAL;
    }        
    
    wake_up_interruptible(&indicator_write_wait);

    mutex_lock(&led->region.access_mutex);             
    if (copy_from_user(led->region.reg, buf, region_size))
    {
        dev_err(dev, "can't copy data from user to local region\n");
        mutex_unlock(&led->region.access_mutex);
        return -EFAULT;
    }
    region_write(dev);
    mutex_unlock(&led->region.access_mutex);
    
    return count;  
}

//-----------------------------------------------------------------------------

// Ожидание очереди символьным устройством.
static unsigned int indicator_poll(struct file * filp,
                            struct poll_table_struct * wait)
{
    unsigned int mask = 0;

    poll_wait(filp, &indicator_read_wait, wait);
    poll_wait(filp, &indicator_write_wait, wait);
    
    /* 
     * It's always possible to read data from region, 
     * because there are some information registers.
     * 
     * It's is also possible at any time to write to
     * the region to configure the behavior of the 
     * IP core.  
     */
    mask |= POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
    
    return mask;
}

//-----------------------------------------------------------------------------

// Структура файлового API символьного устройства.
static const struct file_operations fops =
{
     .owner     = THIS_MODULE,
     .open      = indicator_open,
     .release   = indicator_release,
     .read      = indicator_read,
     .write     = indicator_write,
     .poll      = indicator_poll     
};

//-----------------------------------------------------------------------------
//  Реализации функций SysFS.
//-----------------------------------------------------------------------------

// Аргументы функции чтения.
#define SYSFS_ARGS_SHOW struct device * dev,                            \
                        struct device_attribute * attr,                 \
                        char * buf

// Аргументы функции записи.
#define SYSFS_ARGS_STORE struct device * dev,                           \
                         struct device_attribute * attr,                \
                         const char * buf, size_t count

//-----------------------------------------------------------------------------

// Сигнатура функции чтения.
#define SYSFS_SHOW(_func_name_)                                        \
    static ssize_t indicator_##_func_name_##_show(SYSFS_ARGS_SHOW)

// Сигнатура функции записи.
#define SYSFS_STORE(_func_name_)                                       \
    static ssize_t indicator_##_func_name_##_store(SYSFS_ARGS_STORE)

//-----------------------------------------------------------------------------

// Задать атрибуты функции для чтения/записи.
#define INDICATOR_DEVICE_ATTR_RW(_name)                                        \
    DEVICE_ATTR_RW(indicator_##_name)

// Задать атрибуты функции только для чтения.
#define INDICATOR_DEVICE_ATTR_RO(_name)                                        \
    DEVICE_ATTR_RO(indicator_##_name)

//-----------------------------------------------------------------------------

/* indicator-led main file */
SYSFS_SHOW(indicator_led)
{
    u32 mask = 0xFFU;     /* 7-0 bits */
    return sysfs_read(dev, buf, INDICATOR_OFFSET,
                    mask);
}

SYSFS_STORE(indicator_led)
{
    u32 mask = 0xFFU;     /* 7-0 bits */
    return sysfs_write(dev, buf, count, 
                    INDICATOR_OFFSET,
                    mask);
}
 
static INDICATOR_DEVICE_ATTR_RW(indicator_led);

//-----------------------------------------------------------------------------

// Структура API каталога SysFS.
static struct attribute * indicator_attrs[] =
{
    &dev_attr_indicator_indicator_led.attr,            
    NULL
};

static const struct attribute_group indicator_attrs_group =
{
    .attrs = indicator_attrs,
};

//-----------------------------------------------------------------------------
// Для деинициализации драйвера.
//-----------------------------------------------------------------------------

// Этапы деинициализации.
enum ip_core_clean 
{
    IP_CORE_CLEAN_GROUP,
    IP_CORE_DELETE_DEVICE,
    IP_CORE_DESTROY_DEVICE,
    IP_CORE_UNREGISTER_REGION,
    IP_CORE_CLEAN_INITIAL    
};

//-----------------------------------------------------------------------------

// Деинициализация драйвера.
void cleanup_handler(struct platform_device * pdev, enum ip_core_clean index)
{
    struct device  * dev     = &pdev->dev;
    struct ip_core * ipcore  = dev_get_drvdata(dev);
    
    dev_dbg(dev, "cleanup handler function called\n");
    
    switch (index)
    {
    case IP_CORE_CLEAN_GROUP:
        sysfs_remove_group(&ipcore->device->kobj, &indicator_attrs_group);
        /* fall through */

    case IP_CORE_DELETE_DEVICE:
        cdev_del(&ipcore->char_device);
        /* fall through */

    case IP_CORE_DESTROY_DEVICE:
        device_destroy(ipcore_driver_class, ipcore->devt);
        /* fall through */

    case IP_CORE_UNREGISTER_REGION:
        unregister_chrdev_region(ipcore->devt, 1);
        /* fall through */

    case IP_CORE_CLEAN_INITIAL:
        dev_set_drvdata(dev, NULL);
        break;

    default:
        dev_warn(dev, "can't make cleanup, unknown clean-index #%d\n", 
                (int) index);
        break;
    }    
}
 
//-----------------------------------------------------------------------------

// Инициализация драйвера.
static int ip_core_probe(struct platform_device * pdev)
{
    int ret = -1;                           /* error return value */
    struct resource * r_mem  = NULL;         /* IO mem resources */
    struct ip_core * ipcore  = NULL;         /* represents IP Core */
    struct device * dev      = &pdev->dev;   /* OS device (from device tree) */
    char * device_name       = NULL;         /* unique name of the device */
    char * tmp_device_name   = NULL;
    size_t i;
        
    dev_dbg(dev, "probe function called\n");
    
    /* allocate device wrapper memory */
    ipcore = devm_kmalloc(dev, sizeof (*ipcore), GFP_KERNEL);
    if (IS_ERR(ipcore))
    {
        dev_err(dev, "can't allocate memmory for device");
        return PTR_ERR(ipcore);
    }

    dev_set_drvdata(dev, ipcore);
    ipcore->dt_device = dev;
            
    //-------------------------------------------------------------------------
    //   init device memory space
    //-------------------------------------------------------------------------
    
    /* get iospace for the device */
    r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (IS_ERR(r_mem))
    {
        dev_err(dev, "can't get memory resources\n");
        cleanup_handler(pdev, IP_CORE_CLEAN_INITIAL);
        return PTR_ERR(r_mem);       
    }

    ipcore->mem = r_mem;
    
    /* request physical memory and map to kernel virtual address space */
    ipcore->io_base = devm_ioremap_resource(&pdev->dev, ipcore->mem);
    if (IS_ERR(ipcore->io_base))
    {
        dev_err(dev, "can't lock and map memmory region\n");
        cleanup_handler(pdev, IP_CORE_CLEAN_INITIAL);
        return PTR_ERR(ipcore->io_base);
    }

    dev_info(dev, "got memory location [%pa - %pa]", &ipcore->mem->start, 
            &ipcore->mem->end);
    dev_info(dev, "remapped memory to 0x%p\n", ipcore->io_base);
    
    /* init mutex */
    mutex_init(&ipcore->region.access_mutex);
    
    /* print the initial values of the region  */
    region_read(dev);
    /* don't need lock mutex, because make debug-print in init function */
    for (i = 0; i < REGISTER_COUNT; i++)
    {
        dev_dbg(dev, "region reg%d = %x\n", i, ipcore->region.reg[i]);
    }
    
    //-------------------------------------------------------------------------
    //   init char device
    //-------------------------------------------------------------------------
    
    /* create unique device name  */
    device_name = devm_kmalloc(dev, DRIVER_NAME_LEN, GFP_KERNEL);
    tmp_device_name = devm_kmalloc(dev, DRIVER_NAME_LEN, GFP_KERNEL);
    snprintf(tmp_device_name, DRIVER_NAME_LEN, "%pad", &ipcore->mem->start);
    snprintf(device_name, DRIVER_NAME_LEN, "%s_%s", DRIVER_NAME, 
            &tmp_device_name[2]);
    
    /* allocate device number */
    ret = alloc_chrdev_region(&ipcore->devt, 0, 1, device_name);
    if (ret < 0)
    {
        dev_err(dev, "can't allocate chrdev region\n");
        cleanup_handler(pdev, IP_CORE_CLEAN_INITIAL);
        return ret;
    }
    else
    {
        dev_dbg(dev, "allocated device number major %i minor %i\n",
            MAJOR(ipcore->devt), MINOR(ipcore->devt));
    }
    
    /* create driver file */
    ipcore->device = device_create(ipcore_driver_class, NULL, ipcore->devt,
                                NULL, device_name);
    if (IS_ERR(ipcore->device))
    {
        dev_err(dev, "can't create driver file -> %s\n", device_name);
        cleanup_handler(pdev, IP_CORE_UNREGISTER_REGION);
        return PTR_ERR(ipcore->device);
    }

    dev_set_drvdata(ipcore->device, ipcore);
    
    /* create character device */
    cdev_init(&ipcore->char_device, &fops);
    ret = cdev_add(&ipcore->char_device, ipcore->devt, 1);
    if (ret < 0)
    {
        dev_err(dev, "can't create character device\n");
        cleanup_handler(pdev, IP_CORE_DESTROY_DEVICE);
        return ret;
    }

    //-------------------------------------------------------------------------
    // create sysfs entries
    //-------------------------------------------------------------------------
     
    ret = sysfs_create_group(&ipcore->device->kobj, &indicator_attrs_group);        
    if (ret < 0)
    {
        dev_err(dev, "can't register sysfs group\n");
        cleanup_handler(pdev, IP_CORE_DELETE_DEVICE);
        return ret;                                                    
    }
    
    return 0;
}

//-----------------------------------------------------------------------------

// Выгрузка драйвера IP-Core.
static int ip_core_remove(struct platform_device * pdev)
{
    dev_dbg(&pdev->dev, "remove function called\n");
    cleanup_handler(pdev, IP_CORE_CLEAN_GROUP);    
    
    return 0;
}

//-----------------------------------------------------------------------------

// Таблица идентификации устройства IP-Core.
static const struct of_device_id ip_core_of_match[] =
{
    { .compatible = "xlnx,indicator_module-1.0"},
    {},
};
MODULE_DEVICE_TABLE(of, ip_core_of_match);

//-----------------------------------------------------------------------------

// Структура устройства IP-Core.
static struct platform_driver ip_core_driver =
{
    .driver =
    {
        .name           = DRIVER_NAME,
        .of_match_table = ip_core_of_match,        
    },
    .probe  = ip_core_probe,
    .remove = ip_core_remove,    
};

//-----------------------------------------------------------------------------

// Инициализация модуля.
static int __init indicator_init(void)
{    
    pr_info("%s loaded\n", DRIVER_NAME);
    
    /* create class for char device */
    ipcore_driver_class = class_create(THIS_MODULE, DRIVER_NAME);
    if (IS_ERR(ipcore_driver_class))
    {
        printk(KERN_ERR "Can't create char device class \"%s\"\n", DRIVER_NAME);
        return PTR_ERR(ipcore_driver_class);
    }
    
    return platform_driver_register(&ip_core_driver);   
}

//-----------------------------------------------------------------------------

// Деинициализация модуля.
static void __exit indicator_exit(void)
{
    platform_driver_unregister(&ip_core_driver);
    class_destroy(ipcore_driver_class);
    pr_info("%s unloaded\n", DRIVER_NAME);
}

//-----------------------------------------------------------------------------

module_init(indicator_init);
module_exit(indicator_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kirill Yustitskii <inst: yustitskii_kirill>");
MODULE_DESCRIPTION("SSRS led-indicator driver");
