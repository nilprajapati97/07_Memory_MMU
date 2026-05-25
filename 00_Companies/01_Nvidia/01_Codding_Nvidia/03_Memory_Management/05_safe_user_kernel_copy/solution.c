// Safe user-kernel buffer copy code
#include <linux/uaccess.h>

ssize_t safe_copy_from_user(void *to, const void __user *from, size_t n) {
    if (copy_from_user(to, from, n))
        return -EFAULT;
    return n;
}

ssize_t safe_copy_to_user(void __user *to, const void *from, size_t n) {
    if (copy_to_user(to, from, n))
        return -EFAULT;
    return n;
}
