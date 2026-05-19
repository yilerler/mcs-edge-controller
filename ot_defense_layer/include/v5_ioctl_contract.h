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
    #include <linux/ioctl.h> // ★ 關鍵修復：確保 Debian 抓得到 _IOWR 巨集
#endif

/* ==========================================================
 * IT/OT 互鎖通訊合約 (24 Bytes)
 * ========================================================== */
#pragma pack(push, 1)
typedef struct {
    uint8_t  ot_system_level;  
    uint8_t  fence_status;     
    uint16_t fire_heat_value;  
    uint32_t fence_distance;   

    uint8_t  it_door_request;  
    uint8_t  reserved_it_1;    
    uint16_t reserved_it_2;    
    uint32_t rfid_card_hash;   

    uint8_t  actual_door_state;
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
