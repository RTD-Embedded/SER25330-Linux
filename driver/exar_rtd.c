
/**
    @file

    @brief
        PCIe EXAR UART GPIO driver source code

        This code is based on and inspired by code from the original 
        Exar GPIO driver. Therefore I give credit here for that creators 
        contribution, but since this person isn't responsible for my
        less than perfect implementation I do not want them to get requests
        for any help on them.
        This person is the progenitor of the idea but this code is not their
        responsibility. 

credit:
MODULE_AUTHOR("Sudip Mukherjee <sudipm.mukherjee@gmail.com>");

    $Id:$
 */
#include<linux/kernel.h>
#include <linux/miscdevice.h>
#include<linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pci.h>

MODULE_LICENSE("GPL");

static LIST_HEAD(rtd_list);

#define MPIOLVL15_8  0x96
#define MPIOLVL7_0   0x90
#define MPIOSEL15_8  0x99
#define MPIOSEL7_0   0x93
// Defines the mask for the MPIO pin connected to the EPLD that allows EEPROM
//  access when pulled low.
#define MPIO_MASK_EEPROM_ACCESS 0x10

#define MAX_BOARDS 8

struct ourdev {
	struct miscdevice *misc;
	struct gpio_chip *chip;
	struct list_head list;
};

struct pci_dev *exar_chip[MAX_BOARDS] =
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
void __iomem *exar_bar0[MAX_BOARDS] =
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
#define bar0access(offset) *(u8*)(exar_bar0[chip->base]+(offset))

static ssize_t device_read(struct file *filp, char __user *buff,
			   size_t len, loff_t *off)
{
	int ret;
	u16 value = 0;
	struct ourdev *odev = filp->private_data;
	struct gpio_chip *chip = odev->chip;

	if (exar_chip[chip->base] == NULL) {
		return ENXIO;
	}
	value = bar0access(MPIOLVL15_8);
	value = (value << 8) | (bar0access(MPIOLVL7_0));
	//      printk(KERN_INFO "Value from read %04x\n",value);
	ret = copy_to_user(buff, &value, 2);

	return ret;
}

static ssize_t device_write(struct file *filp,
			    const char __user *buff, size_t len, loff_t *off)
{
	/* 
	 * buf[0] = dir/port,
	 * buf[1] = mask 0 - 7 
	 * buf[2] = mask 8 - 15
	 * buf[3] = value 0 - 7
	 * buf[4] = value 8 - 15
	 */

	u8 buf[5];		/* buf[0]= dir/port, buf[1]=mask, buf[2]=value */
	u8 val;
	struct ourdev *odev = filp->private_data;
	struct gpio_chip *chip = odev->chip;

	if (len != 5) {
		pr_err("len = %d\n", (int)len);
		return -EINVAL;
	}
	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] != 'd' && buf[0] != 'p')
		return -EINVAL;

	if (buf[0] == 'd') {
		// printk(KERN_INFO "buf 0  %02x\n",buf[0]);
		// printk(KERN_INFO "buf 1  %02x\n",buf[1]);
		// printk(KERN_INFO "buf 2  %02x\n",buf[2]);
		// printk(KERN_INFO "buf 3  %02x\n",buf[3]);
		// printk(KERN_INFO "buf 4  %02x\n",buf[4]);
		// set the direction for the low 8 bits
		val = bar0access(MPIOSEL7_0);
		val &= ~buf[1];
		val |= (buf[3] & buf[1]);
		bar0access(MPIOSEL7_0) = val;
		val = bar0access(MPIOSEL15_8);
		val &= ~buf[2];
		val |= (buf[4] & buf[2]);
		bar0access(MPIOSEL15_8) = val;

	} else {		/* out port pins */
		val = bar0access(MPIOLVL7_0);
		val &= ~buf[1];
		val |= (buf[1] & buf[3]);
		bar0access(MPIOLVL7_0) = val;
		val = bar0access(MPIOLVL15_8);
		val &= ~buf[1];
		val |= (buf[1] & buf[3]);
		bar0access(MPIOLVL15_8) = val;
	}

	return len;
}

static int device_open(struct inode *inode, struct file *filp)
{
	struct ourdev *odev;
	bool found = false;
	struct gpio_chip *chip;

	list_for_each_entry(odev, &rtd_list, list) {
		if (odev->misc->minor == iminor(inode)) {
			found = true;
			break;
		}
	}
	if (!found) {
		pr_err("Some thing bad happened, we cannot find the device\n");
		return -ENODEV;
	}

	chip = odev->chip;
	// now we are going to open a mapping to the card and
	// use that to access input and output. 
	// this is not best practices, this a brute force way
	// to get what I need for my customers.
	// odev->chip->base
	exar_bar0[chip->base] =
	    ioremap_cache(pci_resource_start(exar_chip[chip->base], 0),
			  pci_resource_len(exar_chip[chip->base], 0));
	if (exar_bar0[chip->base] == NULL) {
		printk(KERN_INFO "Mapping BAR0 mem map'd registers failed");
		pr_err("Some thing bad happened!\n");
		return -ENODEV;
	}
	filp->private_data = odev;

	pr_err("open done\n");
	return 0;

}

static int device_close(struct inode *inode, struct file *filp)
{
	struct ourdev *odev = filp->private_data;
	struct gpio_chip *chip = odev->chip;
	u8 val;

	if (exar_bar0[chip->base] == NULL) {
		pr_err("closing a non open memory space??\n");
	} else {
		// Set the 'EEPROM access' bit high        
		val = bar0access(MPIOLVL7_0);
		val &= ~MPIO_MASK_EEPROM_ACCESS;
		val |= MPIO_MASK_EEPROM_ACCESS;
		bar0access(MPIOLVL7_0) = val;
		bar0access(MPIOSEL7_0) = 0xff;

		printk(KERN_INFO "Gpio port should be closed\n");
		exar_bar0[chip->base] = NULL;
	}

	return 0;
}

static const struct file_operations dev_fops = {
	.owner = THIS_MODULE,
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_close,
};

static int __init init_m(void)
{
	struct miscdevice *misc;
	struct ourdev *odev;
	struct gpio_chip *chip;
	int cnt = 0;
	int ret;
	char *buf;
	struct pci_dev *dev;
	unsigned long memlog;
	void __iomem *bar0 = NULL;

	// start search for the PCI device 
	// and find all pci devices
	dev = pci_get_device(0x13A8, 0x0358, NULL);

	do {

		if (!dev) {
			printk(KERN_INFO "Failed to locate Exar UART device\n");
			break;
		}

		printk(KERN_INFO
		       "PCI Device located [connected to driver %s]\n",
		       dev->driver->name);

		memlog = pci_resource_start(dev, 0);	// get BAR0 base address..
		printk(KERN_INFO "BAR0 = %08lx \n", memlog);
		bar0 =
		    ioremap_cache(pci_resource_start(dev, 0),
				  pci_resource_len(dev, 0));
		if (bar0 == NULL) {
			printk(KERN_INFO
			       "Mapping BAR0 mem map'd registers failed");

		} else {
			printk(KERN_INFO "BAR0 pointer = %p \n", bar0);
			printk(KERN_INFO "BAR0 Revision  = %02x \n",
			       *(u8 *) (bar0 + 0x8c));
			printk(KERN_INFO "BAR0 DeviceID  = %02x \n",
			       *(u8 *) (bar0 + 0x8D));
			iounmap(bar0);
		}

//              chip = gpiochip_find(NULL, gpio_match);
		// In this driver we are going to create fake "chip" structures
		// to keep the general code operation, but we are going 
		// to create additional records that will serve as a list of
		// pci device offsets , allowing us to actually access the chip
		// properly.
		chip = kzalloc(sizeof(*chip), GFP_KERNEL);
		if (!chip) {
			pci_dev_put(dev);
			return -ENOMEM;
		}

		buf = kzalloc(12, GFP_KERNEL);
		if (!buf) {
			ret = -ENOMEM;
			goto err_free_chip;
		}

		misc = kzalloc(sizeof(*misc), GFP_KERNEL);
		if (!misc) {
			ret = -ENOMEM;
			goto err_free_buf;
		}

		odev = kzalloc(sizeof(*odev), GFP_KERNEL);
		if (!odev) {
			ret = -ENOMEM;
			goto err_free_misc;
		}

		sprintf(buf, "exar-rtd%d", cnt);

		misc->minor = MISC_DYNAMIC_MINOR;
		misc->name = buf;
		misc->fops = &dev_fops, misc->mode = 0666;

		ret = misc_register(misc);
		if (ret < 0) {
			pr_err("misc register failed with code %d\n", ret);
			goto err_free_odev;
		}

		exar_chip[cnt] = dev;
		chip->base = cnt++;

		INIT_LIST_HEAD(&odev->list);
		odev->misc = misc;
		odev->chip = chip;
		list_add(&odev->list, &rtd_list);

		dev = pci_get_device(0x13A8, 0x0358, dev);
	} while ((dev != NULL) && (cnt < MAX_BOARDS));

	if (list_empty(&rtd_list))
		return -ENODEV;

	return 0;

err_free_odev:
	kfree(odev);
err_free_misc:
	kfree(misc);
err_free_buf:
	kfree(buf);
err_free_chip:
	kfree(chip);
	pci_dev_put(dev);
	return ret;
}

static void __exit cleanup_m(void)
{
	struct ourdev *temp;
	struct ourdev *odev;

	printk(KERN_INFO "Removing all entries for this driver.\n");
	list_for_each_entry_safe(odev, temp, &rtd_list, list) {
		list_del(&odev->list);
		misc_deregister(odev->misc);
		printk(KERN_INFO "Removing device index %02d \n",
		       odev->chip->base);
		pci_dev_put(exar_chip[odev->chip->base]);
		exar_chip[odev->chip->base] = NULL;
		kfree(odev->misc->name);
		kfree(odev->misc);
		kfree(odev->chip);
		kfree(odev);
	}
}

module_init(init_m);
module_exit(cleanup_m);
MODULE_AUTHOR("Anthony Marchini <techsupport@rtd.com>");
