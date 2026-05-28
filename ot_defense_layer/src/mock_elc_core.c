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

#define CRITICAL_DISTANCE_MM 500 // 圍籬危險閥值 (小於 50cm 觸發局部防禦)

struct elc_device {
    spinlock_t lock;                   
    v5_ioctl_contract_t reg_map;       
    struct task_struct *polling_thread;
};

static struct elc_device my_elc;

// =========================================================
// ⚙️ M2: 電子圍籬輪詢引擎 (Layer 1 防禦 - Kthread)
// =========================================================
static int fence_polling_thread(void *data) {
    unsigned long flags;
    uint32_t mock_distance;
    
    while (!kthread_should_stop()) {
        mock_distance = 300 + (get_random_u32() % 1000);

        spin_lock_irqsave(&my_elc.lock, flags);

        my_elc.reg_map.fence_distance = mock_distance;
        my_elc.reg_map.ot_heartbeat_ms = jiffies_to_msecs(jiffies);

        // 🛡️ 🌟 [V5.2.1 重構] 狀態機互鎖邏輯：不再使用魔術數字 0, 1, 2
        // 絕對不能覆蓋火警 (EMERGENCY)！
        if (my_elc.reg_map.ot_system_level != V5_STATE_EMERGENCY) {
            if (mock_distance < CRITICAL_DISTANCE_MM) {
                if (my_elc.reg_map.ot_system_level != V5_STATE_WARNING) {
                    printk(KERN_WARNING "[OT M2] ⚠️ 圍籬闖入 (距離: %d mm)！觸發局部防禦。\n", mock_distance);
                    my_elc.reg_map.ot_system_level = V5_STATE_WARNING;
                    my_elc.reg_map.fence_status = V5_FENCE_BRAKING; // 🌟 具象化煞車狀態
                }
            } else {
                if (my_elc.reg_map.ot_system_level == V5_STATE_WARNING) {
                    printk(KERN_INFO "[OT M2] 🟢 圍籬淨空。恢復常態營運。\n");
                    my_elc.reg_map.ot_system_level = V5_STATE_NORMAL;
                    my_elc.reg_map.fence_status = V5_FENCE_CLEAR;   // 🌟 具象化淨空狀態
                }
            }
        }

        spin_unlock_irqrestore(&my_elc.lock, flags);
        usleep_range(50000, 55000); 
    }
    return 0;
}

// =========================================================
// 🚨 M1: 火警中斷模擬器 (Layer 0 絕對武力 - Sysfs)
// =========================================================
static struct kobject *v5_kobj;

static ssize_t level_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", my_elc.reg_map.ot_system_level);
}

static ssize_t level_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    int new_level;
    unsigned long flags;

    // 🌟 [V5.2.1 重構] Sysfs 介面語意對齊 IT (0=正常, 3=火警)
    if (sscanf(buf, "%d", &new_level) == 1 && (new_level == V5_STATE_EMERGENCY || new_level == V5_STATE_NORMAL)) {
        spin_lock_irqsave(&my_elc.lock, flags);
        
        my_elc.reg_map.ot_system_level = new_level;
        if (new_level == V5_STATE_EMERGENCY) {
            printk(KERN_EMERG "[OT M1] 🔥 偵測到火警硬體中斷！啟動 EMERGENCY 全域霸王條款！\n");
            my_elc.reg_map.actual_door_state = V5_DOOR_FORCE_RELEASED; // 🌟 強制釋放門鎖
        } else {
            printk(KERN_INFO "[OT M1] 🔑 主管轉動實體鑰匙，解除鎖定，恢復 NORMAL 營運。\n");
        }
        
        spin_unlock_irqrestore(&my_elc.lock, flags);
    }
    return count;
}
static struct kobj_attribute level_attribute = __ATTR(level, 0644, level_show, level_store);

// =========================================================
// ⬆️ 北向通訊：IT 橋接器 (IOCTL 處理)
// =========================================================
static long elc_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    v5_ioctl_contract_t local_copy;
    unsigned long flags;

    if (cmd != V5_IOC_EXCHANGE) return -ENOTTY;
    if (copy_from_user(&local_copy, (v5_ioctl_contract_t __user *)arg, sizeof(local_copy))) return -EFAULT;

    spin_lock_irqsave(&my_elc.lock, flags);

    my_elc.reg_map.it_door_request = local_copy.it_door_request;
    my_elc.reg_map.rfid_card_hash  = local_copy.rfid_card_hash;

    // 🌟 [V5.2.1 重構] 業務邏輯：只有在 NORMAL (常態) 才能核准開門
    if (my_elc.reg_map.ot_system_level == V5_STATE_NORMAL) {
        if (my_elc.reg_map.it_door_request == V5_DOOR_GRANTED) {
            my_elc.reg_map.actual_door_state = V5_DOOR_GRANTED; 
        } else {
            my_elc.reg_map.actual_door_state = V5_DOOR_LOCKED;  
        }
    }

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
    
    // 🌟 [V5.2.1 重構] 預設為常態營運
    my_elc.reg_map.ot_system_level = V5_STATE_NORMAL; 
    
    misc_register(&elc_misc_dev);
    v5_kobj = kobject_create_and_add("v5_safety", kernel_kobj);
    sysfs_create_file(v5_kobj, &level_attribute.attr);

    my_elc.polling_thread = kthread_run(fence_polling_thread, NULL, "v5_m2_fence");

    printk(KERN_INFO "[OT Core] 🛡️ V5.2.1 語意強化防禦核心上線！\n");
    return 0;
}

static void __exit elc_core_exit(void) {
    if (my_elc.polling_thread) kthread_stop(my_elc.polling_thread);
    sysfs_remove_file(v5_kobj, &level_attribute.attr);
    kobject_put(v5_kobj);
    misc_deregister(&elc_misc_dev);
    printk(KERN_INFO "[OT Core] 🛑 核心卸載。\n");
}

module_init(elc_core_init);
module_exit(elc_core_exit);
MODULE_LICENSE("GPL");