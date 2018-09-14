#include<linux/module.h> /*Needed by all modules*/
#include<linux/kernel.h> /*Needed for KERN_INFO*/
#include<linux/init.h>   /*Needed for the macros*/
#include<linux/interrupt.h>
#include<linux/sched.h>
#include<linux/platform_device.h>
#include<linux/io.h>
#include<linux/of.h>
#include<linux/miscdevice.h>
#include<linux/fs.h>
#include<linux/uaccess.h>
#include<linux/slab.h>
#include<linux/dma-mapping.h>
#include<linux/ctype.h>


#define DRIVER_NAME                "bus access"
#define DEV_FILE_NAME_MAX_LENGTH    64

struct device_data_t
{
    void __iomem*     regs;
    unsigned int      address;
    unsigned int      size;
    struct miscdevice dev;
    char              file_name[DEV_FILE_NAME_MAX_LENGTH];
};

static int open( struct inode *n, struct file *f )
{
   return 0;
}

loff_t llseek(struct file *filp, loff_t f_pos, int whence)
{

    struct device_data_t* device_data;

    printk(KERN_INFO DRIVER_NAME": llseek 0x%x  0x%x bytes\n", (int)f_pos,(int) whence);
    device_data=container_of(filp->private_data,struct device_data_t, dev);

    if(!device_data)
    {
      printk(KERN_ERR DRIVER_NAME": internal error\n");
      return -EIO;
    }

    switch(whence)
    {
      case 0:
      break;

      case 1:
        f_pos+=filp->f_pos;
      break;

      case 2:
        f_pos+=device_data->size;
      break;

      default:
        f_pos=-1;
      break;
    }

    if(f_pos<0)
      return -EINVAL;

    filp->f_pos=f_pos;

    return filp->f_pos;
}

ssize_t read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    unsigned long inner_buff[4096/4];
    struct device_data_t* device_data;
    int idx;

    device_data=container_of(filp->private_data,struct device_data_t, dev);
    if(!device_data)
    {
      printk(KERN_ERR DRIVER_NAME": internal error\n");
      return -EIO;
    }

    printk(KERN_INFO DRIVER_NAME": try to read from 0x%x  0x%x bytes\n", (unsigned int)*f_pos, count);

    if(*f_pos >= device_data->size)
      return 0;

    if(count > device_data->size-*f_pos)
      count = device_data->size-*f_pos;

    if(count > sizeof(inner_buff))
      count = sizeof(inner_buff);

    printk(KERN_INFO DRIVER_NAME": read %d\n", count);
#if 0
    for(idx=0; idx<count; idx+=4)
    {
      inner_buff[idx>>2]=ioread32(device_data->regs+*f_pos+idx);
    }

    if(copy_to_user(buf,inner_buff,count))
    {
        return -EFAULT;
    }
#endif
    *f_pos+=count;
    return count;
}

ssize_t write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    unsigned long inner_buff[4096/4];
    struct device_data_t* device_data;
    int idx;

    device_data=container_of(filp->private_data,struct device_data_t, dev);
    if(!device_data)
    {
      printk(KERN_ERR DRIVER_NAME": internal error\n");
      return -EIO;
    }

    printk(KERN_INFO DRIVER_NAME": try to write to 0x%x  0x%x bytes\n", (unsigned int)*f_pos, count);


    if(*f_pos >= device_data->size)
      return 0;

    if(count > device_data->size-*f_pos)
      count = device_data->size-*f_pos;

    if(count > sizeof(inner_buff))
      count = sizeof(inner_buff);

#if 0
    if(copy_from_user(inner_buff,buf,count))
    {
        return -EFAULT;
    }

    for(idx=0; idx<count; idx+=4)
    {
      iowrite32(inner_buff[idx>>2], device_data->regs+*f_pos+idx);
    }
#endif
    *f_pos+=count;

    return count;
}






static const struct file_operations fops = {
   .owner  = THIS_MODULE,
   .open   = open,
   .llseek = llseek,
   .read   =  read,
   .write  =  write,
};

static int bus_access_driver_probe(struct platform_device* pdev)
{
    int result=0;
    int idx;
    struct device_data_t* device_data;
    struct resource* dev_resource = NULL;
    struct resource *res = NULL;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    printk(KERN_INFO DRIVER_NAME":get resorce start: 0x%08x  size: 0x%08x name: %s\n", res->start, resource_size(res), res->name);


    device_data = devm_kzalloc(&pdev->dev, sizeof(struct device_data_t),GFP_KERNEL);
    dev_resource = &pdev->resource[0];

    idx=0;
    while(dev_resource->name[idx])
    {
      device_data->file_name[idx]=dev_resource->name[idx];
      ++idx;
    }
    device_data->file_name[idx]='\0';


    device_data->dev.minor=MISC_DYNAMIC_MINOR;
    device_data->dev.fops=&fops;
    device_data->dev.mode=0666;
    device_data->dev.name=device_data->file_name;

    result= misc_register(&device_data->dev);


    device_data->regs = devm_ioremap_resource(&pdev->dev, res);

    if (IS_ERR(device_data->regs))
    {
        result = PTR_ERR(device_data->regs);
        printk(KERN_ERR DRIVER_NAME":ioremap error\n");
	goto misc_deregister;
    }

    platform_set_drvdata(pdev,device_data);
    printk(KERN_INFO DRIVER_NAME": %s registered. Start address: %x   length: %x\n", device_data->file_name, res->start, resource_size(res));

    return 0;

misc_deregister:
    misc_deregister(&device_data->dev);
    return result;
}

static int bus_access_driver_remove(struct platform_device* pdev)
{
    struct device_data_t* device_data;
    device_data=platform_get_drvdata(pdev);
    if(device_data)
    {
      misc_deregister(&device_data->dev);
    }
    printk(KERN_INFO" removed\n");
    platform_set_drvdata(pdev,NULL);
    return 0;
}

static const struct of_device_id bus_access_driver_id[]=
{
	{.compatible="alex,bus_access"},
	{}
};

static struct platform_driver bus_access_driver=
{
	.driver=
	{
		.name=DRIVER_NAME,
		.owner=THIS_MODULE,
		.of_match_table=of_match_ptr(bus_access_driver_id),
	},
	.probe=bus_access_driver_probe,
	.remove=bus_access_driver_remove
};

module_platform_driver(bus_access_driver);
MODULE_LICENSE("GPL");
