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


#define DEVNAME                     "bus access"
#define DEV_FILE_NAME_MAX_LENGTH    64

struct device_data_t
{
    void __iomem*     regs;
    unsigned int      address;
    unsigned int      size;
    struct miscdevice dev;
    char              file_name[DEV_FILE_NAME_MAX_LENGTH];
    struct miscdevice dev_cmd;
    char              file_name_cmd[DEV_FILE_NAME_MAX_LENGTH];
};

static int open( struct inode *n, struct file *f )
{
   return 0;
}

static int open_cmd( struct inode *n, struct file *f )
{
   return 0;
}


loff_t llseek(struct file *filp, loff_t f_pos, int whence)
{

    struct device_data_t* device_data;

    printk(KERN_INFO DEVNAME": llseek 0x%x  0x%x bytes\n", (int)f_pos,(int) whence);
    device_data=container_of(filp->private_data,struct device_data_t, dev);

    if(!device_data)
    {
      printk(KERN_ERR DEVNAME": internal error\n");
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
      printk(KERN_ERR DEVNAME": internal error\n");
      return -EIO;
    }

    printk(KERN_INFO DEVNAME": try to read from 0x%x  0x%x bytes\n", (unsigned int)*f_pos, count);

    if(*f_pos >= device_data->size)
      return 0;

    count=min(count, device_data->size-*f_pos);
    count=min(count, sizeof(inner_buff));

    printk(KERN_INFO DEVNAME": read %d\n", count);

    for(idx=0; idx<count; idx+=4)
    {
      inner_buff[idx>>2]=ioread32(device_data->regs+*f_pos+idx);
    }

    if(copy_to_user(buf,inner_buff,count))
    {
        return -EFAULT;
    }

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
      printk(KERN_ERR DEVNAME": internal error\n");
      return -EIO;
    }

    if(*f_pos >= device_data->size)
      return 0;

    count=min(count, device_data->size-*f_pos);
    count=min(count, sizeof(inner_buff));


    if(copy_from_user(inner_buff,buf,count))
    {
        return -EFAULT;
    }

    for(idx=0; idx<count; idx+=4)
    {
      iowrite32(inner_buff[idx>>2], device_data->regs+*f_pos+idx);
    }

    *f_pos+=count;

    return count;
}



ssize_t write_cmd(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    char mess[64];
    int can_continue;
    struct device_data_t* device_data;
    unsigned int address, value, idx;
    int address_defined, value_defined;

    device_data=container_of(filp->private_data,struct device_data_t, dev_cmd);
    if(!device_data)
    {
      printk(KERN_ERR DEVNAME": internal error\n");
      return -EIO;
    }

    if(count>sizeof(mess))
    {
      return -EINVAL;
    }

    if(copy_from_user(mess,buf,count))
    {
        return -EFAULT;
    }

    address=value=idx=0;
    address_defined=value_defined=0;

    printk(KERN_INFO DEVNAME": write length %d\n", count);

    while(mess[idx]==' ') ++idx;

    can_continue=1;
    while((idx<count) && can_continue)
    {
      if(isdigit(mess[idx]))
      {
        address=address*10+(mess[idx]-'0');
        address_defined=1;
      }
      else
      {
        if(mess[idx]!=' ' && mess[idx]!='\n')
        {
          printk(KERN_INFO DEVNAME": bad char %d (idx %d)\n", mess[idx], idx);
          return -EFAULT;
        }
        can_continue=0;
      }
      ++idx;
    }

    printk(KERN_INFO DEVNAME": address defined\n");

    while(mess[idx]==' ') ++idx;

    can_continue=1;
    while((idx<count) && can_continue)
    {
      if(isdigit(mess[idx]))
      {
        value=value*10+(mess[idx]-'0');
        value_defined=1;
      }
      else
      {
        if(mess[idx]!=' ' && mess[idx]!='\n')
        {
          printk(KERN_INFO DEVNAME": bad char %d (idx %d)\n", mess[idx], idx);
          return -EFAULT;
        }
        can_continue=0;
      }
      ++idx;
    }
    printk(KERN_INFO DEVNAME": write to address %d value %d\n", address, value);
    printk(KERN_INFO DEVNAME": defined address %d value %d\n", address_defined, value_defined);

    if(address_defined && value_defined  && (address < device_data->size))
      iowrite32(value, device_data->regs+(address>>2));
    else
      return -EFAULT;

    return count;
}


static const struct file_operations fops = {
   .owner  = THIS_MODULE,
   .open   = open,
   .llseek = llseek,
//   .release = mopen_release,
   .read   =  read,
   .write  =  write,
};


static const struct file_operations fops_cmd = {
   .owner  = THIS_MODULE,
   .open =    open_cmd,
//   .release = mopen_release,
//   .read   =  read,
   .write  =  write_cmd,
};

static int bus_access_driver_probe(struct platform_device* pdev)
{
    int result=0;
    int idx;
    struct device_data_t* device_data;
    struct resource* dev_resource=NULL;
    struct resource *res=NULL;
    void* ptr;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    printk(KERN_INFO DEVNAME":get resorce start: %x  end: %x name: %s\n", res->start, res->end, res->name);



    device_data = devm_kzalloc(&pdev->dev, sizeof(struct device_data_t),GFP_KERNEL);

    dev_resource = &pdev->resource[0];

    idx=0;
    while(dev_resource->name[idx])
    {
      device_data->file_name[idx]=dev_resource->name[idx];
      device_data->file_name_cmd[idx]=dev_resource->name[idx];
      ++idx;
    }
    device_data->file_name[idx]='\0';

    device_data->file_name_cmd[idx++]='_';
    device_data->file_name_cmd[idx++]='c';
    device_data->file_name_cmd[idx++]='m';
    device_data->file_name_cmd[idx++]='d';
    device_data->file_name_cmd[idx++]='\0';


    device_data->dev_cmd.minor=MISC_DYNAMIC_MINOR;
    device_data->dev_cmd.fops=&fops_cmd;
    device_data->dev_cmd.mode=0666;

    device_data->dev=device_data->dev_cmd;
    device_data->dev.fops=&fops;

    device_data->dev_cmd.name=device_data->file_name_cmd;
    device_data->dev.name=device_data->file_name;


    result= misc_register(&device_data->dev_cmd);
    result= misc_register(&device_data->dev);

    device_data->address=0;
    ptr=of_get_property(pdev->dev.of_node,"alex,start-address",NULL);
    if(ptr)
    {
        device_data->address=be32_to_cpup(ptr);
     	printk(KERN_INFO DEVNAME":start address: %x\n", device_data->address);
    }
    else
        printk(KERN_ERR DEVNAME":start address not found\n");


    device_data->size=0;
    ptr=of_get_property(pdev->dev.of_node,"alex,address-space",NULL);
    if(ptr)
    {
        device_data->size=be32_to_cpup(ptr);
     	printk(KERN_INFO DEVNAME":address space: %x\n", device_data->size);
    }
    else
        printk(KERN_ERR DEVNAME":addres space not found\n");

    if(!device_data->address || !device_data->size)
    {
        printk(KERN_ERR DEVNAME":addres space not found\n");
        result=-EIO;
        goto misc_deregister;
    }

    device_data->regs=ioremap_nocache(device_data->address,device_data->size);
    if(!device_data->regs)
    {
        printk(KERN_ERR DEVNAME":ioremap error\n");
        result=-EIO;
        goto misc_deregister;
    }


    platform_set_drvdata(pdev,device_data);
    printk(KERN_INFO DEVNAME": %s registered. Start address: %x   length: %x\n", device_data->file_name_cmd, device_data->address, device_data->size);

    return 0;

misc_deregister:
    misc_deregister(&device_data->dev_cmd);
    misc_deregister(&device_data->dev);

    return result;
}

static int bus_access_driver_remove(struct platform_device* pdev)
{
    struct device_data_t* device_data;
    device_data=platform_get_drvdata(pdev);
    if(device_data)
    {
      misc_deregister(&device_data->dev_cmd);
      misc_deregister(&device_data->dev);
      iounmap(device_data->regs);
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
		.name=DEVNAME,
		.owner=THIS_MODULE,
		.of_match_table=of_match_ptr(bus_access_driver_id),
	},
	.probe=bus_access_driver_probe,
	.remove=bus_access_driver_remove
};

module_platform_driver(bus_access_driver);
MODULE_LICENSE("GPL");
