#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <mach/platform.h>
#include <asm/uaccess.h>
#include <asm/io.h>



MODULE_AUTHOR("Per Olofsson <magervalp@fastmail.fm>");
MODULE_DESCRIPTION("Raspberry Pi fifogfx driver");
MODULE_LICENSE("Dual MIT/GPL");



#define FIFOGFX_DEVICE_NAME "fifogfx"
#define FIFOGFX_NDEVICES 1

static unsigned int fifogfx_major = 0;
//static struct fifogfx_dev *fifogfx_devices = NULL;
static struct class *fifogfx_class = NULL;
static bool gpio_pins_allocated = false;
static struct cdev fifogfx_cdev;
static unsigned char fifogfx_buffer[0x10000] = {0};
static int gpio_irq_num = -EINVAL;
static bool emulate_c64_memmap = true;
static unsigned char fifogfx_c64io[0x1000] = {0};

#define GPIO_RD_PIN 18
#define GPIO_RAH_PIN 17
#define GPIO_RAL_PIN 16
#define GPIO_EF_PIN 19
#define GPIO_DATA_PINS 20

static struct gpio gpio_pins[] = {
    {GPIO_RD_PIN, GPIOF_OUT_INIT_HIGH, "FIFO read D"},
    {GPIO_RAH_PIN, GPIOF_OUT_INIT_HIGH, "FIFO read AH"},
    {GPIO_RAL_PIN, GPIOF_OUT_INIT_HIGH, "FIFO read AL"},
    {GPIO_EF_PIN, GPIOF_IN, "FIFO empty flag"},
    {GPIO_DATA_PINS + 0, GPIOF_IN, "DATA0"},
    {GPIO_DATA_PINS + 1, GPIOF_IN, "DATA1"},
    {GPIO_DATA_PINS + 2, GPIOF_IN, "DATA2"},
    {GPIO_DATA_PINS + 3, GPIOF_IN, "DATA3"},
    {GPIO_DATA_PINS + 4, GPIOF_IN, "DATA4"},
    {GPIO_DATA_PINS + 5, GPIOF_IN, "DATA5"},
    {GPIO_DATA_PINS + 6, GPIOF_IN, "DATA6"},
    {GPIO_DATA_PINS + 7, GPIOF_IN, "DATA7"},
};


struct pi_gpio_reg {
    uint32_t GPFSEL[6];         // Function select.
    uint32_t _gp_reserved_18;
    uint32_t GPSET[2];          // Output set.
    uint32_t _gp_reserved_24;
    uint32_t GPCLR[2];          // Output clear.
    uint32_t _gp_reserved_30;
    uint32_t GPLEV[2];          // Level.
    uint32_t _gp_reserved_3C;
    uint32_t GPEDS[2];          // Event detect status.
    uint32_t _gp_reserved_48;
    uint32_t GPREN[2];          // Rising edge detect enable.
    uint32_t _gp_reserved_54;
    uint32_t GPFEN[2];          // Falling edge detect enable.
    uint32_t _gp_reserved_60;
    uint32_t GPHEN[2];          // High detect enable.
    uint32_t _gp_reserved_6C;
    uint32_t GPLEN[2];          // Low detect enable.
    uint32_t _gp_reserved_78;
    uint32_t GPAREN[2];         // Async rising edge detect enable.
    uint32_t _gp_reserved_84;
    uint32_t GPAFEN[2];         // Async falling edge detect enable.
    uint32_t _gp_reserved_90;
    uint32_t GPPUD;             // Pull-up/down enable.
    uint32_t GPPUDCLK[2];       // Pull-up/down enable clock.
};
static struct pi_gpio_reg volatile * const pi_gpio_reg = (struct pi_gpio_reg *)__io_address(GPIO_BASE);


static uint32_t fifo_empty(void) {
    return ((ioread32(&pi_gpio_reg->GPLEV[0]) >> GPIO_EF_PIN) & 1) == 0;
}

static uint32_t fifo_read(uint32_t selectpin) {
    iowrite32(1 << selectpin, &pi_gpio_reg->GPCLR[0]);
    // Read twice instead of calling ndelay().
    //ndelay(35);
    uint32_t data = (ioread32(&pi_gpio_reg->GPLEV[0]) >> GPIO_DATA_PINS) & 0xff;
    data = (ioread32(&pi_gpio_reg->GPLEV[0]) >> GPIO_DATA_PINS) & 0xff;
    iowrite32(1 << selectpin, &pi_gpio_reg->GPSET[0]);
    // No delay is necessary on Pi Zero, the next I/O write takes 24 ns.
    //ndelay(20);
    return data;
}

static irqreturn_t irq_handler(int irq, void *context) {
    int count = 0;
    
    // printk is *not* safe to call from within an irq handler, remove in
    // production code.
    //printk(KERN_DEBUG "fifogfx irq triggered\n");
    
    pi_gpio_reg->GPSET[0] |= (1 << GPIO_RAH_PIN) | (1 << GPIO_RAL_PIN);
    
    while (!fifo_empty()) {
        uint32_t data = fifo_read(GPIO_RD_PIN);
        uint32_t addr = (fifo_read(GPIO_RAH_PIN) << 8) | fifo_read(GPIO_RAL_PIN);
        if (emulate_c64_memmap \
                && (addr & 0xf000) == 0xd000 \
                && (fifogfx_buffer[1] & 7) >= 5) {
            fifogfx_c64io[addr & 0x0fff] = data;
        } else {
            fifogfx_buffer[addr] = data;
        }
        //uint32_t data = fifo_read(GPIO_RD_PIN);
        //uint32_t addr = fifo_read(GPIO_RAL_PIN);
        //if (addr == 0x0400 || addr == 0xd800) {
        //    printk(KERN_DEBUG "fifogfx write %04x:%02x\n", addr, data);
        //}
        ++count;
    }
    //printk(KERN_DEBUG "fifogfx irq processed %d bytes\n", count);
    
    return IRQ_HANDLED;
}


static irqreturn_t quick_check_handler(int irq, void *context) {
    // Change to IRQ_NEEDS_HANDLING to run irq_handler immediately.
    return IRQ_WAKE_THREAD;
}


int fifogfx_open(struct inode *inode, struct file *filp) {
	unsigned int mj = imajor(inode);
	unsigned int mn = iminor(inode);
	
	if (mj != fifogfx_major || mn < 0 || mn >= FIFOGFX_NDEVICES) {
		printk(KERN_WARNING
            "No fifogfx device found with minor=%d and major=%d\n", 
			mn, mj);
		return -ENODEV;
	}
	
	if (inode->i_cdev != &fifogfx_cdev) {
		printk(KERN_WARNING "open: internal error\n");
		return -ENODEV;
	}
	
    filp->private_data = NULL;
    
	return 0;
}


int fifogfx_release(struct inode *inode, struct file *filp) {
    return 0;
}


ssize_t fifogfx_read(struct file *filp, char __user *buf, size_t count,	loff_t *f_pos) {
    if (count > sizeof(fifogfx_buffer)) {
        count = sizeof(fifogfx_buffer);
    }
    
	if (copy_to_user(buf, fifogfx_buffer, count) != 0) {
		return -EFAULT;
	}
	
	return count;
}


struct file_operations fifogfx_fops = {
	.owner =    THIS_MODULE,
	.read =     fifogfx_read,
	.open =     fifogfx_open,
	.release =  fifogfx_release,
};


// static int fifogfx_dev_uevent(struct device *dev, struct kobj_uevent_env *env) {
//     add_uevent_var(env, "DEVMODE=%#o", 0666);
//     return 0;
// }


static int fifogfx_construct_device(int minor, struct class *class) {
    int err = 0;
    dev_t devno = MKDEV(fifogfx_major, minor);
    struct device *device = NULL;
    
    // printk(KERN_DEBUG "fifogfx->dev_uevent was %p\n", class->dev_uevent);
    // class->dev_uevent = fifogfx_dev_uevent;
    
    cdev_init(&fifogfx_cdev, &fifogfx_fops);
    fifogfx_cdev.owner = THIS_MODULE;
    
    if ((err = cdev_add(&fifogfx_cdev, devno, 1))) {
        printk(KERN_ERR "fifogx adding /dev/%s%d failed with error %d",
            FIFOGFX_DEVICE_NAME, minor, err);
        return err;
    }
    
    device = device_create(class, NULL, // no parent
        devno, NULL,
        FIFOGFX_DEVICE_NAME "%d", minor);
    if (IS_ERR(device)) {
        err = PTR_ERR(device);
        printk(KERN_ERR "fifogfx creating /dev/%s%d failed with error %d",
            FIFOGFX_DEVICE_NAME, minor, err);
        cdev_del(&fifogfx_cdev);
        return err;
    }
    
    printk(KERN_NOTICE "fifogfx created /dev/%s%d",
            FIFOGFX_DEVICE_NAME, minor);
    return 0;
}


static void fifogfx_destroy_device(int minor, struct class *class) {
	device_destroy(class, MKDEV(fifogfx_major, minor));
	cdev_del(&fifogfx_cdev);
}


static void fifogfx_cleanup_module(void) {
    if (gpio_irq_num >= 0) {
        free_irq(gpio_irq_num, NULL);
    }
    
    if (gpio_pins_allocated) {
        gpio_free_array(gpio_pins, ARRAY_SIZE(gpio_pins));
    }
    
    // FIXME: if
    fifogfx_destroy_device(0, fifogfx_class);
    
    if (fifogfx_class) {
		class_destroy(fifogfx_class);
    }
    
    if (fifogfx_major) {
	    unregister_chrdev_region(MKDEV(fifogfx_major, 0), FIFOGFX_NDEVICES);
    }
}


static int __init fifogfx_init(void) {
    int result = 0;
    int err = 0;
    dev_t dev = 0;
    int count = 0;
    uint32_t data = 0;
    uint32_t addr = 0;
    
    printk(KERN_DEBUG "fifogfx module init\n");
    
    if ((result = alloc_chrdev_region(&dev, 0, FIFOGFX_NDEVICES, FIFOGFX_DEVICE_NAME)) < 0) {
        printk(KERN_ERR "fifogfx chrdev region allocation failed with error %d\n", result);
        fifogfx_cleanup_module();
        return result;
    }
    fifogfx_major = MAJOR(dev);

    if (IS_ERR(fifogfx_class = class_create(THIS_MODULE, FIFOGFX_DEVICE_NAME))) {
        err = PTR_ERR(fifogfx_class);
        printk(KERN_ERR "fifogfx class creation failed with error %d\n", err);
        fifogfx_cleanup_module();
        return err;
    }
    
    if ((result = fifogfx_construct_device(0, fifogfx_class)) < 0) {
        printk(KERN_ERR "fifogfx device construction failed with error %d\n", result);
        fifogfx_cleanup_module();
        return result;
    }
    
    if ((result = gpio_request_array(gpio_pins, ARRAY_SIZE(gpio_pins))) < 0) {
        printk(KERN_ERR "fifogfx gpio request failed with error %d\n", result);
        fifogfx_cleanup_module();
        return result;
    }
    gpio_pins_allocated = true;
    
    if ((gpio_irq_num = gpio_to_irq(GPIO_EF_PIN)) < 0) {
        printk(KERN_ERR "fifogfx failed to map irq to gpio %d with error %d\n",
               GPIO_EF_PIN, gpio_irq_num);
        fifogfx_cleanup_module();
        return gpio_irq_num;
    }
    
    printk(KERN_DEBUG "fifogfx mapped interrupt %d to gpio %d\n",
           gpio_irq_num, GPIO_EF_PIN);

    while (!fifo_empty() && count < 4096) {
        data = fifo_read(GPIO_RD_PIN);
        addr = (fifo_read(GPIO_RAH_PIN) << 8) | fifo_read(GPIO_RAL_PIN);
        fifogfx_buffer[addr] = data;
        ++count;
    }
    fifo_read(GPIO_RD_PIN);
    fifo_read(GPIO_RAH_PIN);
    fifo_read(GPIO_RAL_PIN);
    fifo_read(GPIO_RD_PIN);
    fifo_read(GPIO_RAH_PIN);
    fifo_read(GPIO_RAL_PIN);
    printk(KERN_DEBUG "fifogfx read %d bytes at startup\n", count);
    
    if ((result = request_threaded_irq(gpio_irq_num,
                                       irq_handler,
                                       quick_check_handler,
                                       IRQF_TRIGGER_RISING,
                                       FIFOGFX_DEVICE_NAME,
                                       NULL)) < 0) {
        printk(KERN_ERR "fifogfx failed to request irq with error %d\n", result);
        fifogfx_cleanup_module();
        return result;
    }
    
    printk(KERN_DEBUG "fifogfx using gpio registers at %p\n", pi_gpio_reg);
    
    return 0;
}


static void __exit fifogfx_exit(void) {
    printk(KERN_DEBUG "fifogfx module exiting\n");
    
    fifogfx_cleanup_module();
}


module_init(fifogfx_init);
module_exit(fifogfx_exit);
