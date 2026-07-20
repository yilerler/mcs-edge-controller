#ifndef V5_IOCTL_CONTRACT_H
#define V5_IOCTL_CONTRACT_H

/* ==========================================================
 * 跨環境標頭檔載入 (Cross-Environment Includes)
 * ========================================================== */
#ifdef __KERNEL__
    #include <linux/types.h>
    #include <linux/ioctl.h>
#else
    #include <stdint.h>
    #include <sys/ioctl.h>
    #include <linux/ioctl.h>
#endif

/* ==========================================================
 * 🌟 語意合約區 (Semantic Dictionary) - [V5.2.4 解耦瘦身版]
 * ========================================================== */

// 全域系統防禦等級 (OT System Level)
#define V5_STATE_NORMAL     0  
#define V5_STATE_WARNING    1  
#define V5_STATE_DEGRADED   2  
#define V5_STATE_EMERGENCY  3  

// 圍籬實體狀態 (Fence Status)
#define V5_FENCE_CLEAR   0  
#define V5_FENCE_BRAKING 1  

/* ==========================================================
 * 純物理防禦通訊合約 (12 Bytes 完美對齊)
 * 移除所有 RFID 與門禁邏輯，交還 User Space 處理
 * ========================================================== */
#pragma pack(push, 1)
typedef struct {
    uint8_t  ot_system_level;  // 1 Byte
    uint8_t  fence_status;     // 1 Byte
    uint16_t fire_heat_value;  // 2 Bytes
    uint32_t fence_distance;   // 4 Bytes
    uint32_t ot_heartbeat_ms;  // 4 Bytes
} v5_ioctl_contract_t;         // 總計: 12 Bytes
#pragma pack(pop)

/* ==========================================================
 * IOCTL 系統呼叫指令定義 (Magic Numbers)
 * ========================================================== */
#define V5_IOC_MAGIC 'V' 
#define V5_IOC_EXCHANGE _IOWR(V5_IOC_MAGIC, 1, v5_ioctl_contract_t)

/* ==========================================================
 * 架構防禦：靜態斷言 (Static Assert)
 * ========================================================== */
#if defined(__cplusplus)
    static_assert(sizeof(v5_ioctl_contract_t) == 12, "FATAL: ABI Size Mismatch!");
#elif !defined(__KERNEL__)
    _Static_assert(sizeof(v5_ioctl_contract_t) == 12, "FATAL: ABI Size Mismatch!");
#endif

#endif // V5_IOCTL_CONTRACT_H