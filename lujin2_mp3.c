#define LINUX
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include "mp3_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_ID");
MODULE_DESCRIPTION("CS-423 MP3");

#define DEBUG 1
#define DIRECTORY "mp3"
#define FILENAME "status"
#define REGISTRATION 'R'
#define DEREGISTRATION 'D'


/*
Define a structure to represent Process Control Block
*/
typedef struct mp3_task_struct {
    struct task_struct *task;
    struct list_head task_node;
    unsigned int pid;
} mp3_task_struct;

// Declare proc filesystem entry
static struct proc_dir_entry *proc_dir, *proc_entry;
// Create a list head
LIST_HEAD(my_head);
// Define a spin lock
static DEFINE_SPINLOCK(sp_lock);

/*
Find task struct by pid
*/
mp3_task_struct* find_mptask_by_pid(unsigned long pid)
{
    mp3_task_struct* task;
    list_for_each_entry(task, &my_head, task_node) {
        if(task->pid == pid){
            return task;
        }
    }
    return NULL;
}

static void mp3_register(unsigned int pid) {
    mp3_task_struct *curr_task = (mp3_task_struct *)kmalloc(sizeof(mp3_task_struct), GFP_KERNEL);
    printk(KERN_ALERT "TASK %u REGISTRATION MODULE LOADING\n", pid);

    curr_task->task = find_task_by_pid(pid);
    curr_task->pid = pid;
    set_task_state(curr_task->task, TASK_UNINTERRUPTIBLE);

    // add the task to task list
    unsigned long flags; 
    spin_lock_irqsave(&sp_lock, flags);
    list_add(&(curr_task->task_node), &my_head);
    spin_unlock_irqrestore(&sp_lock, flags);
    printk(KERN_ALERT "TASK %u REGISTRATION MODULE LOADED\n", pid);
}

static void mp3_deregister(unsigned int pid) {
    #ifdef DEBUG
    printk(KERN_ALERT "TASK %u DEREGISTRATION MODULE LOADING\n", pid);
    #endif
    mp3_task_struct *stop;
    unsigned long flags; 

    spin_lock_irqsave(&sp_lock, flags);
    stop = find_mptask_by_pid(pid);
    set_task_state(stop->task, TASK_UNINTERRUPTIBLE);
    list_del(&(stop->task_node));
    spin_unlock_irqrestore(&sp_lock, flags);
    kfree(stop);
    printk(KERN_ALERT "TASK %u DEREGISTRATION MODULE LOADED\n", pid);
}

/*
This function is called then the /proc file is read
filp: file pointer
buf: points to the user buffer holding the data to be written or the empty buffer
count: the size of the requested data transfer
offp: a pointer to a “long offset type” object that indicates the file position the user is accessing
*/
ssize_t mp3_read (struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
    int copied = 0;
    mp3_task_struct *curr;
    char *buffer;
    unsigned long flags; 

    // if the file is already read
    if(*offp > 0){
        return 0;
    }

    //allocate memory in buffer and set the content in buffer as 0
    buffer = (char *)kmalloc(4096, GFP_KERNEL);
    memset(buffer, 0, 4096);

    // Acquire the mutex
    spin_lock_irqsave(&sp_lock, flags);

    // record the location of current node inside "copied" after each entry
    list_for_each_entry(curr, &my_head, task_node){
        copied += sprintf(buffer + copied, "%u\n", curr->pid);
        // printk(KERN_ALERT "I AM READING: %s, %u, %u, %u, %lu\n", buffer, curr->pid, curr->task_state, curr-> task_period, curr->process_time);
    }

    // if the message length is larger than buffer size
    if (copied > count){
        kfree(buffer);
        return -EFAULT;
    }
    spin_unlock_irqrestore(&sp_lock, flags);

    // set the end of string character with value 0 (NULL)
    buffer[copied] = '\0';
    copied += 1;
    //Copy a block of data into user space.
    copy_to_user(buf, buffer, copied);

    // Free previously allocated memory
    kfree(buffer);

    *offp += copied;
    return copied;
}


/*
his function is called with the /proc file is written
filp: file pointer
buf: points to the user buffer holding the data to be written or the empty buffer
count: the size of the requested data transfer
offp: a pointer to a “long offset type” object that indicates the file position the user is accessing
*/
ssize_t mp3_write (struct file *filp, const char __user *buf, size_t count, loff_t *offp)
{
    char *buffer = (char *)kmalloc(4096, GFP_KERNEL);
    unsigned int pid;

    memset(buffer, 0, 4096);
    // if the lengh of message is larger than buffer size or the file is already written, return
    if (count > 4096 || *offp>0) {
        kfree(buffer);
        return -EFAULT;
    }

    copy_from_user(buffer, buf, count);
    buffer[count] = '\0';

    if(count > 0){
        switch (buffer[0])
        {
            case REGISTRATION:
                sscanf(buffer + 2, "%u\n", &pid);
                mp3_register(pid);
                break;
            case DEREGISTRATION:
                sscanf(buffer + 2, "%u\n", &pid);
                mp3_deregister(pid);
                break;
            default:
                kfree(buffer);
                return 0;
        }
    }

    printk(KERN_ALERT "I AM WRITING: %s, parse results: pid %u\n", buffer, pid);

    kfree(buffer);
    return count;
}

// Declare the file operations
static const struct file_operations file_fops = {
    .owner = THIS_MODULE,
    .write = mp3_write,
    .read  = mp3_read,
};


// mp3_init - Called when module is loaded
int __init mp3_init(void)
{
   printk(KERN_ALERT "MP3 MODULE LOADING\n");

   //Create proc directory "/proc/mp3/" using proc_dir(dir, parent)
   proc_dir = proc_mkdir(DIRECTORY, NULL);

   //Create file entry "/proc/mp3/status/" using proc_create(name, mode, parent, proc_fops)
   proc_entry = proc_create(FILENAME, 0666, proc_dir, &file_fops);
   
   // init spin lock
   spin_lock_init(&sp_lock);

   printk(KERN_ALERT "MP3 MODULE LOADED\n");
   return 0;
}


// mp3_exit - Called when module is unloaded
void __exit mp3_exit(void)
{
    mp3_task_struct *pos, *next;
    unsigned long flags; 

    printk(KERN_ALERT "MP3 MODULE UNLOADING\n");

    spin_lock_irqsave(&sp_lock, flags);
    // Free the task struct list
    list_for_each_entry_safe(pos, next, &my_head, task_node) {
        list_del(&pos->task_node);
    }
    spin_unlock_irqrestore(&sp_lock, flags);

    /*
    remove /proc/mp3/status and /proc/mp3 using remove_proc_entry(*name, *parent)
    Removes the entry name in the directory parent from the procfs
    */
    remove_proc_entry(FILENAME, proc_dir);
    remove_proc_entry(DIRECTORY, NULL);
    printk(KERN_ALERT "MP3 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp3_init);
module_exit(mp3_exit);