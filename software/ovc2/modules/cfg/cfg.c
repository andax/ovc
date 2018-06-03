#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>

struct cfg {
  struct spi_device *spi_device;
  struct cdev cdev;
  int major_number;
  struct class *dev_class;
  struct device *device;
};
static struct cfg cfg;
//////////////////////////////////////////////////////////////////////

int cfg_open(struct inode *inode, struct file *fp)
{
  u8 msg[2] = {0x01, 0x02};
  spi_write(cfg.spi_device, msg, 2);
  return 0;  // success  
}

ssize_t cfg_write(struct file *f, const char __user *p, size_t count, loff_t *pos)
{
  //int rc;
  printk(KERN_INFO "cfg: cfg_write(%d)\n", (int)count);
  /*
  u8 *tx_buf;
  tx_buf = kmalloc(count, GFP_KERNEL);
  if (!tx_buf) {
    printk("cfg_write(): kmalloc returned NULL\n");
    return -EIO;
  }
  printk("cfg_write(): allocated %d bytes of kernel memory\n",
    (int)ksize(tx_buf));
  kfree(tx_buf);
  */
  // may need to use spi_sync_locked() ?
  // in order to not release SCLK/MOSI between 128k bursts
  // if so, need to copy out helper code from /include/linux/spi/spi.h
  // (the body of spi_write() is just a few lines)
  //rc = spi_write(cfg.spi_device, p, count); //&msg, 4);
  //if (
  return count;
}

int cfg_release(struct inode *inode, struct file *file)
{
  return 0;  // success
}

static long cfg_ioctl(struct file *file, unsigned int ioctl_num,
  unsigned long ioctl_param)
{
  return -EINVAL;  // no ioctl defined yet
}

struct file_operations cfg_fops = {
  .owner          = THIS_MODULE,
  //.read           = cfg_read,
  .write          = cfg_write,
  .open           = cfg_open,
  .release        = cfg_release,
  .unlocked_ioctl = cfg_ioctl,
};

#define DEVICE_NAME "ovc_cfg"
#define CLASS_NAME  "ovc_cfg"

static int __init cfg_init(void)
{
  int rc;
  //u32 msg = 0x12345678;
  struct spi_master *master;
  struct spi_board_info board_info = {
    //.modalias = "cfg",
    .max_speed_hz = 10000000,
    .bus_num = 3,
    .chip_select = 0,
    .mode = SPI_LSB_FIRST  // | SPI_NO_CS
  };

  cfg.major_number = register_chrdev(0, DEVICE_NAME, &cfg_fops);
  if (cfg.major_number < 0) {
    printk(KERN_ERR "cfg: register_chrdev() failed\n");
    return cfg.major_number;
  }
  printk(KERN_INFO "cfg: registered major number %d\n", cfg.major_number);

  cfg.dev_class = class_create(THIS_MODULE, CLASS_NAME);
  if (IS_ERR(cfg.dev_class)) {
    unregister_chrdev(cfg.major_number, DEVICE_NAME);
    printk(KERN_ERR "cfg: class_create() failed\n");
    return PTR_ERR(cfg.dev_class);
  }

  cfg.device = device_create(cfg.dev_class, NULL, MKDEV(cfg.major_number, 0),
    NULL, DEVICE_NAME);
  if (IS_ERR(cfg.device)) {
    class_destroy(cfg.dev_class);
    unregister_chrdev(cfg.major_number, DEVICE_NAME);
    printk(KERN_ERR "cfg: device_create() failed\n");
    return PTR_ERR(cfg.device);
  }
  printk(KERN_INFO "cfg: device created successfully\n");

  master = spi_busnum_to_master(board_info.bus_num);
  if (!master) {
    printk("spi_busnum_to_master() failed\n");
    return -ENODEV;
  }
  cfg.spi_device = spi_new_device(master, &board_info);
  if (!cfg.spi_device) {
    printk("spi_new_device() failed\n");
    return -ENODEV;
  }
  cfg.spi_device->bits_per_word = 8;
  rc = spi_setup(cfg.spi_device);
  if (rc) {
    printk("spi_setup() failed\n");
    spi_unregister_device(cfg.spi_device);
    return -ENODEV;
  }
  return 0;
}

static void __exit cfg_exit(void)
{
  /*
  cdev_del(&cfg.cdev);
  unregister_chrdev_region(MKDEV(cfg.major, 0), 1);
  */
  spi_unregister_device(cfg.spi_device);
  device_destroy(cfg.dev_class, MKDEV(cfg.major_number, 0));
  class_unregister(cfg.dev_class);
  class_destroy(cfg.dev_class);
  unregister_chrdev(cfg.major_number, DEVICE_NAME);
  printk(KERN_INFO "cfg: removal complete\n");
}

module_init(cfg_init);
module_exit(cfg_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Morgan Quigley <morgan@openrobotics.org>");
MODULE_DESCRIPTION("SPI configuration driver for Altera Cyclone 10 GX");
