# ADR-0007: 實作基於時間戳記的全域看門狗與降級機制

* **ID:** ADR-0007
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `v5_core_bridge`

---

## 1. Context / Problem (背景與問題)
在分散式微服務架構中，感測器硬體損壞、網路線脫落或微服務節點發生死鎖（Deadlock / Silent Failure）時，進程（Process）可能仍然存活，但資料已停止更新。若核心決策大腦依賴這些「過期的正常數據快取」做出裁決，會導致系統在危險發生時毫無反應（例如誤以為環境依然安全而放行門禁）。

## 2. Decision (決策內容)
在 Core Bridge 中實作基於資料流時間戳記（Data-driven Heartbeat）的全域看門狗機制。一旦發現南向 OT 或同層級感測服務的資料逾時未更新，主動將全域狀態切換為安全降級模式（`STATE_DEGRADED`）。

## 3. Rationale (決策理據)
貫徹 Fail-Safe（失效安全）原則。進程存活不代表邏輯正常運作。基於實際資料送達時間的看門狗機制，能最真實、客觀地反映各模組實際的健康狀況，防範系統基於幻覺（Stale Data）做出致命決策。

## 4. Alternatives (替代方案)
* **Alternative A:** 依賴作業系統層級（如 `systemd`）或 ROS 2 的 Lifecycle Node 來監控進程狀態，若進程 Crash 則自動重啟。

## 5. Rejected Alternatives (拒絕原因)
* **Rejected Alternative A because:** 這些機制只能監控「執行緒是否活著」，無法偵測「感測器是否已被物理拔除」或「無限迴圈死鎖」。靜默崩潰（Silent Failure）會輕易穿透這類 OS 層級的防護網。

## 6. Consequences (預期影響與取捨)
* **Positive:** 徹底消除了依賴過期資料決策的風險，確保系統在任何模組失效時，都會向安全的狀態（鎖死或降級）傾斜。
* **Negative:** 提高了對內部網路穩定度的要求。若 IT 網域內部發生瞬間的 CPU 滿載或 DDS 封包延遲（Jitter），可能會引發看門狗誤判（False Positives），導致不必要的系統頻繁降級與警報。

## 7. Related AKB (關聯架構知識庫)
* [`AKB / 04_Core_Runtime.md`](../AKB/04_Core_Runtime.md)
* **Concept:** Global Watchdog (全域看門狗)、Fail-Safe (失效安全/優雅降級)

## 8. Evidence / References (證據與參考資料)
* 實作證據：`fsm_tick` 定時器中，明確實作了針對 `last_ot_heartbeat_time_`、`last_m4_time_` 的超時判定 `(now - last_time).seconds() > threshold`。