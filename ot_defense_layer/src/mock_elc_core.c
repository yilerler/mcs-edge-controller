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

// 🌟 引入我們剛剛極致瘦身為 12 Bytes 的合約
#include "../include/v5_ioctl_contract.h"

#define CRITICAL_DISTANCE_MM 500 

struct elc_device {
    spinlock_t lock;                   
    v5_ioctl_contract_t reg_map;       
    struct task_struct *polling_thread;
};

static struct elc_device my_elc;

// =========================================================
// ⚙️ M2: 電子圍籬硬體抽象層 (Layer 1 - 純粹的物理距離與狀態)
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
        // (假設有實體火災溫度感測器，也會在這裡讀取 ADC 並寫入 fire_heat_value)

        // 📝 職責 2：僅回報「圍籬實體狀態」，不越權判定系統級別
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
// 🚨 M1: 實體火警/緊急中斷接收器 (Layer 0 - 絕對中斷)
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
// ⬆️ 北向通訊：ICD 合約交換口 (IOCTL)
// =========================================================
static long elc_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    v5_ioctl_contract_t local_copy;
    unsigned long flags;

    if (cmd != V5_IOC_EXCHANGE) return -ENOTTY;
    
    // 雖然目前沒有要從 IT 收資料，但遵循 _IOWR 標準雙向拷貝
    if (copy_from_user(&local_copy, (v5_ioctl_contract_t __user *)arg, sizeof(local_copy))) return -EFAULT;

    spin_lock_irqsave(&my_elc.lock, flags);

    // 🌟 [戰略撤退完成] 
    // 這裡再也沒有 it_door_request，也沒有 rfid_card_hash。
    // LKM 就是個無情的物理狀態快照機。
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
// 初始化與卸載 (具備嚴格錯誤復原機制的 Kernel 級實作)
// =========================================================
static int __init elc_core_init(void) {
    int ret; // 用來承接所有需要嚴格檢查的回傳值

    spin_lock_init(&my_elc.lock);
    memset(&my_elc.reg_map, 0, sizeof(my_elc.reg_map));
    
    my_elc.reg_map.ot_system_level = V5_STATE_NORMAL; 
    
    // 1. 註冊 misc 設備
    ret = misc_register(&elc_misc_dev);
    if (ret) {
        printk(KERN_ERR "[OT_CORE] Failed to register misc device\n");
        return ret;
    }

    // 2. 建立 kobject (若記憶體不足可能會失敗)
    v5_kobj = kobject_create_and_add("v5_safety", kernel_kobj);
    if (!v5_kobj) {
        printk(KERN_ERR "[OT_CORE] Failed to create kobject\n");
        ret = -ENOMEM;
        goto err_misc; // 失敗時，跳到下面去釋放 misc device
    }

    // 3. 建立 sysfs 檔案 (這正是消除 Warning 的關鍵點)
    ret = sysfs_create_file(v5_kobj, &level_attribute.attr);
    if (ret) {
        printk(KERN_ERR "[OT_CORE] Failed to create sysfs file\n");
        goto err_kobj; // 失敗時，跳到下面去釋放 kobject
    }

    // 4. 啟動自主防禦輪詢執行緒
    my_elc.polling_thread = kthread_run(fence_polling_thread, NULL, "v5_m2_fence");
    if (IS_ERR(my_elc.polling_thread)) {
        printk(KERN_ERR "[OT_CORE] Failed to start polling thread\n");
        ret = PTR_ERR(my_elc.polling_thread);
        my_elc.polling_thread = NULL;
        goto err_sysfs; // 失敗時，跳到下面去釋放 sysfs 檔案
    }

    // 🌟 更新宣言：我是 12 Bytes 的純物理防禦盾牌
    printk(KERN_INFO "[OT_CORE] V5 Pure Physical Defense Shield initialized. ICD Payload: 12 Bytes.\n");
    return 0;

// ---------------------------------------------------------
// 錯誤復原區 (Rollback Area) - 依序反向釋放資源
// ---------------------------------------------------------
err_sysfs:
    sysfs_remove_file(v5_kobj, &level_attribute.attr);
err_kobj:
    kobject_put(v5_kobj);
err_misc:
    misc_deregister(&elc_misc_dev);
    return ret;
}

static void __exit elc_core_exit(void) {
    if (my_elc.polling_thread) {
        kthread_stop(my_elc.polling_thread);
    }
    sysfs_remove_file(v5_kobj, &level_attribute.attr);
    kobject_put(v5_kobj);
    misc_deregister(&elc_misc_dev);
    printk(KERN_INFO "[OT_CORE] Module unloaded.\n");
}

module_init(elc_core_init);
module_exit(elc_core_exit);
MODULE_LICENSE("GPL");