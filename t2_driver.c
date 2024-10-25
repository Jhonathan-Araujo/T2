#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>

#define DEVICE_NAME "t2_driver"
#define CLASS_NAME "msgqueue_class"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jhonathan Araújo, Tomás Caldas and William Rodrigues");
MODULE_DESCRIPTION("A message queue driver for each process.");
MODULE_VERSION("1.0.0");

static int majorNumber;
static struct class *charClass = NULL;
static struct device *charDevice = NULL;

// Module parameters
static int max_messages = 10;
module_param(max_messages, int, S_IRUGO);
MODULE_PARM_DESC(max_messages, "Maximum number of messages per process");

static int max_message_size = 128;
module_param(max_message_size, int, S_IRUGO);
MODULE_PARM_DESC(max_message_size, "Maximum size of each message");

// Structure to store messages
struct message_s
{
    struct list_head link;
    char *message;
    size_t size;
};

// Structure to associate a message queue with a process
struct process_queue
{
    pid_t pid;
    char name[50]; // Process name
    struct list_head messages;
    int message_count;
    struct list_head link;
};

static LIST_HEAD(process_list);

// Function to find a process queue by PID
static struct process_queue *find_process_queue_by_pid(pid_t pid)
{
    struct process_queue *entry = NULL;
    list_for_each_entry(entry, &process_list, link)
    {
        if (entry->pid == pid)
        {
            return entry;
        }
    }
    return NULL;
}

// Function to find a process queue by name
static struct process_queue *find_process_queue_by_name(const char *name)
{
    struct process_queue *entry = NULL;
    list_for_each_entry(entry, &process_list, link)
    {
        if (strcmp(entry->name, name) == 0)
        {
            return entry;
        }
    }
    return NULL;
}

// Function to create a new message queue for a process
static struct process_queue *create_process_queue(pid_t pid, const char *name)
{
    struct process_queue *new_queue = kmalloc(sizeof(struct process_queue), GFP_KERNEL);
    if (!new_queue)
    {
        printk(KERN_ALERT "Failed to allocate memory for process queue\n");
        return NULL;
    }
    INIT_LIST_HEAD(&new_queue->messages);
    new_queue->pid = pid;
    strncpy(new_queue->name, name, sizeof(new_queue->name) - 1);
    new_queue->name[sizeof(new_queue->name) - 1] = '\0'; // Ensure null-termination
    new_queue->message_count = 0;
    list_add_tail(&new_queue->link, &process_list);
    printk(KERN_INFO "Process '%s' (PID %d) registered.\n", new_queue->name, new_queue->pid);
    return new_queue;
}

// Function to delete a process's message queue
static void delete_process_queue(struct process_queue *queue)
{
    struct message_s *msg, *tmp;
    list_for_each_entry_safe(msg, tmp, &queue->messages, link)
    {
        list_del(&msg->link);
        kfree(msg->message);
        kfree(msg);
    }
    printk(KERN_INFO "Process '%s' (PID %d) unregistered.\n", queue->name, queue->pid);
    list_del(&queue->link);
    kfree(queue);
}

// Function to add a message to a process's queue
static int add_message_to_queue(struct process_queue *queue, const char *data, size_t len)
{
    if (len > max_message_size)
    {
        printk(KERN_ALERT "Message exceeds maximum size (%d bytes). Discarding.\n", max_message_size);
        return -EINVAL;
    }

    if (queue->message_count >= max_messages)
    {
        printk(KERN_INFO "Queue for '%s' is full. Removing oldest message.\n", queue->name);
        // Remove the oldest message
        struct message_s *oldest = list_first_entry(&queue->messages, struct message_s, link);
        list_del(&oldest->link);
        kfree(oldest->message);
        kfree(oldest);
        queue->message_count--;
    }

    struct message_s *new_msg = kmalloc(sizeof(struct message_s), GFP_KERNEL);
    if (!new_msg)
    {
        printk(KERN_ALERT "Failed to allocate memory for message\n");
        return -ENOMEM;
    }

    new_msg->message = kmalloc(len + 1, GFP_KERNEL); // +1 for null terminator
    if (!new_msg->message)
    {
        printk(KERN_ALERT "Failed to allocate memory for message content\n");
        kfree(new_msg);
        return -ENOMEM;
    }

    strncpy(new_msg->message, data, len);
    new_msg->message[len] = '\0';
    new_msg->size = len;
    list_add_tail(&new_msg->link, &queue->messages);
    queue->message_count++;

    return 0;
}

// Function to read the first message from a process's queue
static ssize_t read_message_from_queue(struct process_queue *queue, char *buffer, size_t len)
{
    if (list_empty(&queue->messages))
    {
        printk(KERN_INFO "No messages for '%s'\n", queue->name);
        return 0; // No message to read
    }

    struct message_s *msg = list_first_entry(&queue->messages, struct message_s, link);
    if (len < msg->size)
    {
        printk(KERN_ALERT "Buffer size too small\n");
        return -EINVAL;
    }

    if (copy_to_user(buffer, msg->message, msg->size))
    {
        printk(KERN_ALERT "Failed to copy message to user space\n");
        return -EFAULT;
    }

    list_del(&msg->link);
    queue->message_count--;
    ssize_t msg_size = msg->size;
    kfree(msg->message);
    kfree(msg);

    return msg_size;
}

// File operations

static int dev_open(struct inode *inodep, struct file *filep)
{
    // Nothing to do here for this driver
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
    pid_t pid = task_pid_nr(current);
    struct process_queue *queue = find_process_queue_by_pid(pid);

    if (!queue)
    {
        printk(KERN_ALERT "Process (PID %d) is not registered. Cannot read messages.\n", pid);
        return -EINVAL;
    }

    return read_message_from_queue(queue, buffer, len);
}

// Function to write data to the device
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
    char *kbuf;
    char command[50];
    char argument[200];
    int ret;

    if (len > 256)
    { // Limit to prevent excessively long input
        printk(KERN_ALERT "Input too long\n");
        return -EINVAL;
    }

    kbuf = kmalloc(len + 1, GFP_KERNEL); // +1 for null terminator
    if (!kbuf)
    {
        printk(KERN_ALERT "Failed to allocate memory for input buffer\n");
        return -ENOMEM;
    }

    if (copy_from_user(kbuf, buffer, len))
    {
        kfree(kbuf);
        printk(KERN_ALERT "Failed to copy data from user space\n");
        return -EFAULT;
    }
    kbuf[len] = '\0';

    // Parse the command
    if (sscanf(kbuf, "/%9s %49[^\n]", command, argument) >= 1)
    {
        pid_t pid = task_pid_nr(current);

        if (strcmp(command, "reg") == 0)
        {
            // Registration
            if (argument[0] == '\0')
            {
                printk(KERN_ALERT "No process name provided for registration\n");
                ret = -EINVAL;
                goto out;
            }

            // Check if the process is already registered
            struct process_queue *existing_queue = find_process_queue_by_pid(pid);
            if (existing_queue)
            {
                printk(KERN_ALERT "Process (PID %d) is already registered as '%s'\n", pid, existing_queue->name);
                ret = -EEXIST;
                goto out;
            }

            // Check if the name is already taken
            if (find_process_queue_by_name(argument))
            {
                printk(KERN_ALERT "Process name '%s' is already in use\n", argument);
                ret = -EEXIST;
                goto out;
            }

            // Create a new process queue
            if (!create_process_queue(pid, argument))
            {
                ret = -ENOMEM;
                goto out;
            }
            ret = len;
            goto out;
        }
        else if (strcmp(command, "unreg") == 0)
        {
            // Unregistration
            struct process_queue *queue = find_process_queue_by_pid(pid);
            if (!queue)
            {
                printk(KERN_ALERT "Process (PID %d) is not registered\n", pid);
                ret = -EINVAL;
                goto out;
            }

            if (strcmp(queue->name, argument) != 0)
            {
                printk(KERN_ALERT "Process name mismatch during unregistration\n");
                ret = -EINVAL;
                goto out;
            }

            // Delete the process queue
            delete_process_queue(queue);
            ret = len;
            goto out;
        }
        else
        {
            // Sending a message to another process
            char *message_content;
            struct process_queue *dest_queue;

            if (argument[0] == '\0')
            {
                printk(KERN_ALERT "No message content provided\n");
                ret = -EINVAL;
                goto out;
            }

            // Find destination process queue
            dest_queue = find_process_queue_by_name(command);
            if (!dest_queue)
            {
                printk(KERN_ALERT "Destination process '%s' is not registered. Discarding message.\n", command);
                ret = -EINVAL;
                goto out;
            }

            // Add message to the destination queue
            ret = add_message_to_queue(dest_queue, argument, strlen(argument));
            if (ret == 0)
            {
                ret = len;
            }
            goto out;
        }
    }
    else
    {
        printk(KERN_ALERT "Invalid command format\n");
        ret = -EINVAL;
    }

out:
    kfree(kbuf);
    return ret;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
    // Nothing to do here for this driver
    return 0;
}

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static int __init msgqueue_init(void)
{
    int result;
    printk(KERN_INFO "Initializing message queue driver\n");

    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0)
    {
        printk(KERN_ALERT "Failed to register major number\n");
        return majorNumber;
    }

    charClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(charClass))
    {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        return PTR_ERR(charClass);
    }

    charDevice = device_create(charClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(charDevice))
    {
        class_destroy(charClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        return PTR_ERR(charDevice);
    }

    printk(KERN_INFO "Message queue driver initialized with max_messages=%d, max_message_size=%d\n",
           max_messages, max_message_size);
    return 0;
}

static void __exit msgqueue_exit(void)
{
    device_destroy(charClass, MKDEV(majorNumber, 0));
    class_unregister(charClass);
    class_destroy(charClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);

    // Free message queues of all processes
    struct process_queue *queue, *tmp;
    list_for_each_entry_safe(queue, tmp, &process_list, link)
    {
        delete_process_queue(queue);
    }

    printk(KERN_INFO "Message queue driver unloaded\n");
}

module_init(msgqueue_init);
module_exit(msgqueue_exit);