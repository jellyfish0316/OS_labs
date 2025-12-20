#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <asm/current.h>

#define procfs_name "Mythread_info"
#define BUFSIZE  1024
char buf[BUFSIZE];

static ssize_t Mywrite(struct file *fileptr, const char __user *ubuf, size_t buffer_len, loff_t *offset){
    /* Do nothing */
	return 0;
}


static ssize_t Myread(struct file *fileptr, char __user *ubuf, size_t buffer_len, loff_t *offset){
    /*Your code here*/
    int len = 0;
    struct task_struct *thread;
    // 1. Check Offset: If we have already read data, return 0 to indicate EOF.
    // The system call might be called multiple times; we only want to send data once.
    // offset = how many bytes have already been read
    if (*offset > 0) {
        return 0;
    }

    // 2. Iterate through threads
    // 'current' points to the process (task_struct) calling the read function
    for_each_thread(current, thread) { // Iterate over all threads of the process that invoked this read operation
        if(thread->tgid == thread->pid ) continue; //tgid = processid -> skip main thread
        // 3. Format the string into the kernel buffer 'buf'
        len += sprintf(buf + len,
                        "PID: %d, TID: %d, Priority: %d, State: %d\n", 
                        current->pid, thread->pid, thread->prio, thread->__state);
    }

    // 4. Copy data to user space
    copy_to_user(ubuf, buf, len);

    // 5. Advance the offset
    *offset += len;

    // Return the number of bytes read
    return len;

    /****************/
}

static struct proc_ops Myops = {
    .proc_read = Myread,
    .proc_write = Mywrite,
};

static int My_Kernel_Init(void){
    proc_create(procfs_name, 0644, NULL, &Myops);   
    pr_info("My kernel says Hi");
    return 0;
}

static void My_Kernel_Exit(void){
    pr_info("My kernel says GOODBYE");
}

module_init(My_Kernel_Init);
module_exit(My_Kernel_Exit);

MODULE_LICENSE("GPL");