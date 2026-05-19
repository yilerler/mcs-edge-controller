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

#define CRITICAL_DISTANCE_MM 500 // 圍籬危險閥值 (小於 50cm 觸發 Level 1)

// =========================================================
// 🗄️ 核心設備結構體 (The Edge Logic Controller)
// =========================================================
struct elc_device {
    spinlock_t lock;                   // 🛡️ 守護 24 Bytes 合約的自旋鎖
    v5_ioctl_contract_t reg_map;       // 📝 全系統唯一真理：24 Bytes 合約
    struct task_struct *polling_thread;// ⚙️ M2 輪詢執行緒
};

static struct elc_device my_elc;

// =========================================================
// ⚙️ M2: 電子圍籬輪詢引擎 (Layer 1 防禦 - Kthread)
// =========================================================
static int fence_polling_thread(void *data) {
    unsigned long flags;
    uint32_t mock_distance;
    
    while (!kthread_should_stop()) {
        // 模擬 HC-SR04 超音波讀數 (產生 300mm ~ 1300mm 的隨機距離)
        mock_distance = 300 + (get_random_u32() % 1000);

        // 🔒 進入中斷安全禁區
        spin_lock_irqsave(&my_elc.lock, flags);

        // 更新遙測數據
        my_elc.reg_map.fence_distance = mock_distance;
        my_elc.reg_map.ot_heartbeat_ms = jiffies_to_msecs(jiffies);

        // 🛡️ 狀態機互鎖邏輯 (Level 1 判定)
        // 注意：絕對不能覆蓋 Level 0 (火警)！只有在 Level 2 才能切換到 Level 1
        if (my_elc.reg_map.ot_system_level != 0) {
            if (mock_distance < CRITICAL_DISTANCE_MM) {
                if (my_elc.reg_map.ot_system_level != 1) {
                    printk(KERN_WARNING "[OT M2] ⚠️ 圍籬闖入 (距離: %d mm)！觸發 Level 1 局部防禦。\n", mock_distance);
                    my_elc.reg_map.ot_system_level = 1;
                    my_elc.reg_map.fence_status = 2; // 煞車中
                }
            } else {
                if (my_elc.reg_map.ot_system_level == 1) {
                    printk(KERN_INFO "[OT M2] 🟢 圍籬淨空。恢復 Level 2 常態營運。\n");
                    my_elc.reg_map.ot_system_level = 2;
                    my_elc.reg_map.fence_status = 0; // 淨空
                }
            }
        }

        // 🔓 解除禁區
        spin_unlock_irqrestore(&my_elc.lock, flags);

        usleep_range(50000, 55000); // 模擬每 50ms 打一發超音波
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

    if (sscanf(buf, "%d", &new_level) == 1 && (new_level == 0 || new_level == 2)) {
        // 🔒 取得自旋鎖 (模擬硬體中斷瞬間凍結其他執行緒)
        spin_lock_irqsave(&my_elc.lock, flags);
        
        my_elc.reg_map.ot_system_level = new_level;
        if (new_level == 0) {
            printk(KERN_EMERG "[OT M1] 🔥 偵測到火警硬體中斷！啟動 LEVEL 0 全域霸王條款！\n");
            my_elc.reg_map.actual_door_state = 2; // 強制釋放門鎖
        } else {
            printk(KERN_INFO "[OT M1] 🔑 主管轉動實體鑰匙，解除 Level 0 鎖定，恢復 Level 2。\n");
        }
        
        // 🔓 釋放自旋鎖
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

    // 🔒 瞬間鎖定，進行 O(1) 決策與深拷貝
    spin_lock_irqsave(&my_elc.lock, flags);

    // 接收 IT 請求
    my_elc.reg_map.it_door_request = local_copy.it_door_request;
    my_elc.reg_map.rfid_card_hash  = local_copy.rfid_card_hash;

    // 業務邏輯：只有在 Level 2 (常態) 且 Level 1 (圍籬) 未觸發時，才允許開門
    if (my_elc.reg_map.ot_system_level == 2) {
        if (my_elc.reg_map.it_door_request == 1) {
            my_elc.reg_map.actual_door_state = 1; // 允許開門
        } else {
            my_elc.reg_map.actual_door_state = 0; // 鎖死
        }
    }

    // 將決策結果拷貝給 IT
    local_copy = my_elc.reg_map;

    // 🔓 釋放鎖
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
    // 1. 初始化資源
    spin_lock_init(&my_elc.lock);
    memset(&my_elc.reg_map, 0, sizeof(my_elc.reg_map));
    my_elc.reg_map.ot_system_level = 2; // 預設 Level 2
    
    // 2. 註冊驅動與 sysfs
    misc_register(&elc_misc_dev);
    v5_kobj = kobject_create_and_add("v5_safety", kernel_kobj);
    sysfs_create_file(v5_kobj, &level_attribute.attr);

    // 3. 啟動 M2 輪詢野獸
    my_elc.polling_thread = kthread_run(fence_polling_thread, NULL, "v5_m2_fence");

    printk(KERN_INFO "[OT Core] 🛡️ V5.1 Sprint 3 最終防禦核心上線！\n");
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