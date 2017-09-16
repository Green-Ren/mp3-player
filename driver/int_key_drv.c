/*
 * Key scan device driver
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <mach/hardware.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>

#include <plat/gpio-cfg.h>
#include <mach/gpio-bank-n.h>
#include <mach/gpio-bank-l.h>

#define USING_TASKLET

static int key_major = 0;
module_param(key_major, int, 0);

struct key_irq_desc {
    	unsigned int irq;
	int number;
    	char *name;	
};

/* ����ָ���������õ��ⲿ�ж����ż��жϴ�����ʽ, ���� */
static struct key_irq_desc key_irqs [] = {
    {IRQ_EINT(0),  0, "KEY1"}, /* K1 */
    {IRQ_EINT(1),  1, "KEY2"}, /* K2 */
    {IRQ_EINT(2),  2, "KEY3"}, /* K3 */
    {IRQ_EINT(3),   3, "KEY4"}, /* K4 */
};

/* ��ֵ��ʼ��) */
static int key_values = 0;

/* �ȴ�����: 
 * ��û�а���������ʱ������н��̵���key_read������
 * ��������
 */
static DECLARE_WAIT_QUEUE_HEAD(key_waitq); //��ʼ��һ���ȴ�����ͷkey_waitq

/* �ж��¼���־, �жϷ����������1��key_read������0 */
static volatile int ev_press = 0;
#ifdef USING_TASKLET

static struct tasklet_struct key_tasklet;
static void key_do_tasklet(unsigned long);
//DECLARE_TASKLET(key_tasklet, key_do_tasklet, 0);

static void key_do_tasklet(unsigned long data)
{
	//printk("key_do_tasklet\n");
}
#endif

static irqreturn_t key_interrupt(int irq, void *dev_id)
{
	struct key_irq_desc *key_irqs = (struct key_irq_desc *)dev_id;
	unsigned tmp;
	int number;
	int up;
	
	number = key_irqs->number;
	tmp = readl(S3C64XX_GPNDAT);
    	up = tmp & (1 << number);

	//printk("<1>up=%d\n",up);
	if (up) {
		ev_press = 0;
		return 0;
	} else {
		key_values = key_irqs->number; //���¼�ֵ
		ev_press = 1;                  /* ��ʾ�жϷ����� */
	}	
    	
    	wake_up_interruptible(&key_waitq);   /* �������ߵĽ��� */
	
#ifdef USING_TASKLET
	tasklet_schedule(&key_tasklet);
#endif
    
    	return IRQ_RETVAL(IRQ_HANDLED);
}


/* Ӧ�ó�����豸�ļ�/dev/keyִ��open(...)ʱ��
 * �ͻ����key_open����
 */
static int key_open(struct inode *inode, struct file *file)
{
    int i;
    int err;
    
    for (i = 0; i < sizeof(key_irqs)/sizeof(key_irqs[0]); i++) {
        // ע���жϴ�������
	//s3c2410_gpio_cfgpin(key_irqs[i].pin,key_irqs[i].pin_setting);
        err = request_irq(key_irqs[i].irq, key_interrupt, 0, 
                          key_irqs[i].name, (void *)&key_irqs[i]);
	set_irq_type(key_irqs[i].irq, IRQ_TYPE_EDGE_FALLING);//<linux/irq.h>
        if (err)
            break;
    }

    if (err) {
        // �ͷ��Ѿ�ע����ж�
        i--;
        for (; i >= 0; i--) {
		disable_irq(key_irqs[i].irq);
            	free_irq(key_irqs[i].irq, (void *)&key_irqs[i]);
        }
        return -EBUSY;
    }
    
    return 0;
}


/* Ӧ�ó�����豸�ļ�/dev/keyִ��close(...)ʱ��
 * �ͻ����key_close����
 */
static int key_close(struct inode *inode, struct file *file)
{
    int i;
    
    for (i = 0; i < sizeof(key_irqs)/sizeof(key_irqs[0]); i++) {
        // �ͷ��Ѿ�ע����ж�
	disable_irq(key_irqs[i].irq);
        	free_irq(key_irqs[i].irq, (void *)&key_irqs[i]);
    }

    return 0;
}


/* Ӧ�ó�����豸�ļ�/dev/keyִ��read(...)ʱ��
 * �ͻ����key_read����
 */
static int key_read(struct file *filp, char __user *buff, 
                        size_t count, loff_t *offp)
{
    unsigned long err;

	if (!ev_press) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		else
			/* ���ev_press����0������,ֱ��key_waitq������,����ev_pressΪ��*/
			wait_event_interruptible(key_waitq, ev_press);
	}
    if(count != sizeof key_values)
	return -EINVAL;
    /* ִ�е�����ʱ��ev_press����1��������0 */
    ev_press = 0;

    /* ������״̬���Ƹ��û�������0 */
    //printk("<1>key_values=%d\n",key_values);
    err = copy_to_user(buff, &key_values, sizeof(key_values));
    return err ? -EFAULT : sizeof(key_values);
}

/**************************************************
* ���û��������select����ʱ��������������
* ����а������ݣ���select���������̷���
* ���û�а������ݣ�������ʹ��poll_wait�ȴ�
**************************************************/
static unsigned int key_poll(struct file *file,
        			 struct poll_table_struct *wait)
{
	unsigned int mask = 0;
    	poll_wait(file, &key_waitq, wait);
    	if (ev_press)
        	mask |= POLLIN | POLLRDNORM;
    	return mask;
}


/* ����ṹ���ַ��豸��������ĺ���
 * ��Ӧ�ó�������豸�ļ�ʱ�����õ�open��read��write�Ⱥ�����
 * ���ջ��������ṹ�еĶ�Ӧ����
 */
static struct file_operations key_fops = {
    .owner   =   THIS_MODULE,    /* ����һ���ָ꣬�����ģ��ʱ�Զ�������__this_module���� */
    .open    =   key_open,
    .release =   key_close, 
    .read    =   key_read,
    .poll    =   key_poll,
};


/*
 * Set up the cdev structure for a device.
 */
static void key_setup_cdev(struct cdev *dev, int minor,
                struct file_operations *fops)
{
        int err, devno = MKDEV(key_major, minor);

        cdev_init(dev, fops);
        dev->owner = THIS_MODULE;
        dev->ops = fops;
        err = cdev_add (dev, devno, 1);
        /* Fail gracefully if need be */
        if (err)
                printk (KERN_NOTICE "Error %d adding key%d", err, minor);
}


/*
 * We export one key device.  There's no need for us to maintain any
 * special housekeeping info, so we just deal with raw cdev.
 */
static struct cdev key_cdev;


/*
 * ִ�С�insmod int_key_drv.ko������ʱ�ͻ�����������
 */
static int __init userkey_init(void)
//static int key_init(void)
{
        int result;
        dev_t dev = MKDEV(key_major, 0);
	char dev_name[]="key";
                                                                                                         
        /* Figure out our device number. */
        if (key_major)
                result = register_chrdev_region(dev, 1, dev_name);
        else {
                result = alloc_chrdev_region(&dev, 0, 1, dev_name);
                key_major = MAJOR(dev);
        }
        if (result < 0) {
                printk(KERN_WARNING "key: unable to get major %d\n", key_major);
                return result;
        }
        if (key_major == 0)
                key_major = result;

#ifdef USING_TASKLET
	tasklet_init(&key_tasklet, key_do_tasklet, 0);
#endif                                                                                                         
        /* Now set up cdev. */
        key_setup_cdev(&key_cdev, 0, &key_fops);
        printk("key device installed, with major %d\n", key_major);
	printk("The device name is: /dev/%s\n", dev_name);
 
    return 0;
}

/*
 * ִ�Сrrmod int_key_drv������ʱ�ͻ����������� 
 */
static void __exit userkey_exit(void)
//static void key_exit(void)
{
        cdev_del(&key_cdev);
        unregister_chrdev_region(MKDEV(key_major, 0), 1);
#ifdef USING_TASKLET
	tasklet_kill(&key_tasklet);
#endif
        printk("key device uninstalled\n");
}

/* ������ָ����������ĳ�ʼ��������ж�غ��� */
module_init(userkey_init);
module_exit(userkey_exit);
EXPORT_SYMBOL(key_major);

/* �������������һЩ��Ϣ�����Ǳ���� */
MODULE_AUTHOR("www.embedclub.com");             // �������������
MODULE_DESCRIPTION("mini6410/tiny6410/S3C6410 KEY Driver");   // һЩ������Ϣ
MODULE_LICENSE("Dual BSD/GPL");                              // ��ѭ��Э��
