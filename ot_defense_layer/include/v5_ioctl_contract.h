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
    #include <linux/ioctl.h> // ★ 確保 Debian 抓得到 _IOWR 巨集
#endif

/* ==========================================================
 * 🌟 語意合約區 (Semantic Dictionary - SSOT)
 * [V5.2.1 升級] 規定所有合法的狀態數值，徹底消除魔術數字污染。
 * 數值已嚴格對齊 ROS 2 端的 SafetyState.msg 定義。
 * ========================================================== */

// 全域系統防禦等級 (OT System Level)
#define V5_STATE_NORMAL     0  // 常態營運 (無異常)
#define V5_STATE_WARNING    1  // 局部警報 (如圍籬闖入、噪音超標)
#define V5_STATE_DEGRADED   2  // 系統降級 (感測器斷線盲區)
#define V5_STATE_EMERGENCY  3  // 全域災難 (火警觸發)

// 大門實體狀態 (Actual Door State / IT Door Request)
#define V5_DOOR_LOCKED         0  // 實體鎖死 / 請求上鎖
#define V5_DOOR_PENDING        1  // 審核中
#define V5_DOOR_GRANTED        2  // 允許通行 / 請求開門
#define V5_DOOR_FORCE_RELEASED 3  // 火警強制釋放

// 圍籬實體狀態 (Fence Status)
#define V5_FENCE_CLEAR   0  // 淨空
#define V5_FENCE_BRAKING 1  // 煞車介入中

/* ==========================================================
 * IT/OT 互鎖通訊合約 (24 Bytes)
 * ========================================================== */
#pragma pack(push, 1)
typedef struct {
    uint8_t  ot_system_level;  // 🛡️ 強制要求：僅能填入 V5_STATE_* 巨集
    uint8_t  fence_status;     // 🛡️ 強制要求：僅能填入 V5_FENCE_* 巨集
    uint16_t fire_heat_value;  
    uint32_t fence_distance;   

    uint8_t  it_door_request;  // 🛡️ 強制要求：僅能填入 V5_DOOR_* 巨集
    uint8_t  reserved_it_1;    
    uint16_t reserved_it_2;    
    uint32_t rfid_card_hash;   

    uint8_t  actual_door_state;// 🛡️ 強制要求：僅能填入 V5_DOOR_* 巨集
    uint8_t  reserved_out_1;   
    uint16_t reserved_out_2;   
    uint32_t ot_heartbeat_ms;  
} v5_ioctl_contract_t;
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
    static_assert(sizeof(v5_ioctl_contract_t) == 24, "FATAL: ABI Size Mismatch!");
#elif !defined(__KERNEL__)
    _Static_assert(sizeof(v5_ioctl_contract_t) == 24, "FATAL: ABI Size Mismatch!");
#endif

#endif // V5_IOCTL_CONTRACT_H