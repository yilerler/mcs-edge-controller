#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/kthread.h>  
#include <linux/delay.h>   
#include <linux/spinlock.h> 
#include <linux/random.h>  

#include "../include/v5_ioctl_contract.h"

#define CRITICAL_DISTANCE_MM 500 

struct elc_device {
    spinlock_t lock;                   
    v5_ioctl_contract_t reg_map;       
    struct task_struct *polling_thread;
};

static struct elc_device my_elc;

// =========================================================
// ⚙️ M2: 電子圍籬硬體抽象層 (Layer 1 - 盲目填寫距離)
// =========================================================
static int fence_polling_thread(void *data) {
    unsigned long flags;
    uint32_t mock_distance;
    
    while (!kthread_should_stop()) {
        mock_distance = 300 + (get_random_u32() % 1000);

        spin_lock_irqsave(&my_elc.lock, flags);

        // 📝 職責 1：如實填寫物理距離與心跳
        my_elc.reg_map.fence_distance = mock_distance;
        my_elc.reg_map.ot_heartbeat_ms = jiffies_to_msecs(jiffies);

        // 🌟 [V5.2.2 拔除越權] 
        // 過去這裡會私自判定 ot_system_level = V5_STATE_WARNING。
        // 現在 OT 喪失宣判危機的權力！它只負責回報物理世界的「圍籬實體狀態 (fence_status)」。
        if (mock_distance < CRITICAL_DISTANCE_MM) {
            if (my_elc.reg_map.fence_status != V5_FENCE_BRAKING) {
                printk(KERN_WARNING "[OT_HAL_M2] Distance < Threshold (%d mm). Setting ICD [fence_status] = V5_FENCE_BRAKING\n", mock_distance);
                my_elc.reg_map.fence_status = V5_FENCE_BRAKING; 
            }
        } else {
            if (my_elc.reg_map.fence_status != V5_FENCE_CLEAR) {
                printk(KERN_INFO "[OT_HAL_M2] Distance OK. Setting ICD [fence_status] = V5_FENCE_CLEAR\n");
                my_elc.reg_map.fence_status = V5_FENCE_CLEAR;   
            }
        }

        spin_unlock_irqrestore(&my_elc.lock, flags);
        usleep_range(50000, 55000); 
    }
    return 0;
}

// =========================================================
// 🚨 M1: 火警中斷接收器 (Layer 0 - 絕對中斷傳遞)
// =========================================================
static struct kobject *v5_kobj;

static ssize_t level_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", my_elc.reg_map.ot_system_level);
}

static ssize_t level_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    int new_level;
    unsigned long flags;

    if (sscanf(buf, "%d", &new_level) == 1 && (new_level == V5_STATE_EMERGENCY || new_level == V5_STATE_NORMAL)) {
        spin_lock_irqsave(&my_elc.lock, flags);
        
        my_elc.reg_map.ot_system_level = new_level;
        
        // 🌟 [V5.2.2 拔除越權]
        // 過去這裡會自作主張寫入 actual_door_state = FORCE_RELEASED。
        // 現在它只負責把 EMERGENCY 旗標立起來，大門要不要開，等 ROS 2 的 FSM 下指令！
        if (new_level == V5_STATE_EMERGENCY) {
            printk(KERN_EMERG "[OT_HAL_M1] HW_IRQ Triggered. Setting ICD [ot_system_level] = V5_STATE_EMERGENCY\n");
        } else {
            printk(KERN_INFO "[OT_HAL_M1] HW_IRQ Cleared. Setting ICD [ot_system_level] = V5_STATE_NORMAL\n");
        }
        
        spin_unlock_irqrestore(&my_elc.lock, flags);
    }
    return count;
}
static struct kobj_attribute level_attribute = __ATTR(level, 0644, level_show, level_store);

// =========================================================
// ⬆️ 北向通訊：ICD 合約交換口
// =========================================================
static long elc_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    v5_ioctl_contract_t local_copy;
    unsigned long flags;

    if (cmd != V5_IOC_EXCHANGE) return -ENOTTY;
    if (copy_from_user(&local_copy, (v5_ioctl_contract_t __user *)arg, sizeof(local_copy))) return -EFAULT;

    spin_lock_irqsave(&my_elc.lock, flags);

    // 接收 IT 的請求
    my_elc.reg_map.it_door_request = local_copy.it_door_request;
    my_elc.reg_map.rfid_card_hash  = local_copy.rfid_card_hash;

    // 🌟 [V5.2.2 拔除越權] 
    // 過去這裡有 if (ot_system_level == NORMAL) 的邏輯裁決。
    // 現在 OT 是無情的繼電器執行者，IT 的 it_door_request 填什麼，它就毫無懸念地推動大門實體狀態。
    my_elc.reg_map.actual_door_state = my_elc.reg_map.it_door_request;

    local_copy = my_elc.reg_map;
    spin_unlock_irqrestore(&my_elc.lock, flags);

    if (copy_to_user((v5_ioctl_contract_t __user *)arg, &local_copy, sizeof(local_copy))) return -EFAULT;
    return 0;
}

static const struct file_operations elc_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = elc_ioctl,
};
static struct miscdevice elc_misc_dev = {
    .minor = MISC_DYNAMIC_MINOR, .name = "v5_safety_core", .fops = &elc_fops, .mode = 0666              
};

// =========================================================
// 初始化與卸載
// =========================================================
static int __init elc_core_init(void) {
    spin_lock_init(&my_elc.lock);
    memset(&my_elc.reg_map, 0, sizeof(my_elc.reg_map));
    
    my_elc.reg_map.ot_system_level = V5_STATE_NORMAL; 
    my_elc.reg_map.actual_door_state = V5_DOOR_LOCKED;
    
    misc_register(&elc_misc_dev);
    v5_kobj = kobject_create_and_add("v5_safety", kernel_kobj);
    sysfs_create_file(v5_kobj, &level_attribute.attr);

    my_elc.polling_thread = kthread_run(fence_polling_thread, NULL, "v5_m2_fence");

    // 🌟 冰冷的機器宣告
    printk(KERN_INFO "[OT_CORE] V5.2.2 Hardware Abstraction Layer initialized. ICD Payload Size: 24 Bytes.\n");
    return 0;
}

static void __exit elc_core_exit(void) {
    if (my_elc.polling_thread) kthread_stop(my_elc.polling_thread);
    sysfs_remove_file(v5_kobj, &level_attribute.attr);
    kobject_put(v5_kobj);
    misc_deregister(&elc_misc_dev);
    printk(KERN_INFO "[OT_CORE] Module unloaded.\n");
}

module_init(elc_core_init);
module_exit(elc_core_exit);
MODULE_LICENSE("GPL");
