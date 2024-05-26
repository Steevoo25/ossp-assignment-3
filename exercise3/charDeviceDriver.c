//WHERE I AM
/*echo works and now saves to array
 *implementation of circular queue complete with checks
 *reeading multiple elements works
 *concurrency works ?
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for put_user */
#include <charDeviceDriver.h>
#include <linux/slab.h>


MODULE_LICENSE("GPL");

DEFINE_MUTEX  (devLock);
DEFINE_MUTEX (rwLock);

char msg[BUF_LEN];
char *messages[1000];
int readPoint= 0, writePoint = 0;

//registers device
int init_module(void)
{
        Major = register_chrdev(0, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk(KERN_ALERT "Registering char device failed with %d\n", Major);
	  return Major;
	}
	
	return SUCCESS;
}

void cleanup_module(void)
{
  
  kfree(messages);//free messages array from heap
	
  unregister_chrdev(Major, DEVICE_NAME); //remove device
}
 
static int device_open(struct inode *inode,
		       struct file *file)
{
  mutex_lock (&devLock); 
    
    if (Device_Open) {
	mutex_unlock (&devLock);
	return -EBUSY;
    }
    
    Device_Open++;
    mutex_unlock (&devLock);
   
    try_module_get(THIS_MODULE);
    
    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
        mutex_lock (&devLock);
	Device_Open--;		/* We're now ready for our next caller */
	mutex_unlock (&devLock);
	/* 
	 * Decrement the usage count, or else once you opened the file, you'll
	 * never get get rid of the module. 
	 */
	module_put(THIS_MODULE);

	return 0;
}


static ssize_t device_read(struct file *filp,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
	/* result of function calls */
	
	int result;
	 
        if (readPoint - writePoint == 0){ //if the array is empty
	   return -EAGAIN;
	}
	
	if (strlen(messages[readPoint%1000]) +1 < length){
	  //make sure buffer is big enough to store message
	  length = strlen(messages[readPoint%1000]) + 1;
	}
	
	mutex_lock (&rwLock); //lock during critical section
	
	result = copy_to_user(buffer, messages[readPoint%1000], length);
	
	if (result > 0){
	  return -EFAULT; //copy failed
        }
	
	kfree(messages[readPoint%1000]); //free memory after message is read
	
	readPoint++; //increment rear pointer of queue
		
	mutex_unlock (&rwLock);//unlock after critical section
	
	return length;
}

static ssize_t device_write(struct file *filp,
		const char *buff, //character buffer of string passed in
		 size_t len, //length of buffer???
		 loff_t * off)
{
  char *kbuff;
  
  if (writePoint-readPoint > 999){//checks if array is full
    return -EBUSY;
  }
  
  if (len > 4096){//checks if buffer is correct length
    return -EINVAL;
  }
  
  kbuff = kmalloc(len+1, GFP_KERNEL); //allocates kernel memory for message
  
  if (kbuff == NULL) { //**memory allocation failes
      printk(KERN_ALERT "Cannot allocate kernel memory\n");
      return -ENOMEM;
  }
  
  mutex_lock (&rwLock); //lock during critical section
  
  if (copy_from_user(kbuff, buff, len)) { 
    printk(KERN_ALERT "Copying from user failed.\n");//copy failed
    kfree(kbuff);
	return -EPERM;
  }
    
    kbuff[len] = '\0'; // prevent buffer overflow
    
    //THE SACRED CODER REJOICES
    messages[writePoint%1000] = kbuff; //assign message into array
    
    writePoint++;//increment front pointer of queue
    mutex_unlock (&rwLock); //unlock once critical section is done
    return len;
}



