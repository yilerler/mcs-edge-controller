# ADR-0009: 於北向閘道器實作優先權佇列 (Priority Queue)

* **ID:** ADR-0009
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `v5_it_gateway`

---

## 1. Context / Problem (背景與問題)
工地邊緣設備的 WAN (如 Wi-Fi 或 4G) 頻寬有限且極度不穩定。當網路發生壅塞或經歷長時斷線後剛恢復連線時，系統會瞬間湧出大量累積的一般環境遙測數據（Telemetry，如每秒的 PM2.5 變化）。如果此時發生了關鍵的工安警報（如人員闖入危險區），警報訊息極容易被海量的遙測數據淹沒或延遲上傳。

## 2. Decision (決策內容)
在北向閘道器（`it_gateway`）的上傳傳輸執行緒中，實作優先權佇列（Priority Queue），對發送至雲端的資料進行流量塑形。

## 3. Rationale (決策理據)
在有限且不穩定的網路資源下，確保系統防禦狀態（Safety State）的變更與緊急警報（Emergency）擁有絕對的頻寬優先使用權，不被一般的非關鍵遙測數據阻塞。

## 4. Alternatives (替代方案)
* **Alternative A:** 使用標準的 FIFO (First-In-First-Out) 佇列。
* **Alternative B:** 收到資料後直接呼叫網路 SDK 發送，不經過任何本地佇列。

## 5. Rejected Alternatives (拒絕原因)
* **Rejected Alternative A/B because:** FIFO 佇列在網路壅塞時會引發 Head-of-Line Blocking（隊頭阻塞）。大量的普通感測數據會卡在佇列前方，導致關鍵的工安警報無法及時送達雲端戰情室，失去預警的時效性。

## 6. Consequences (預期影響與取捨)
* **Positive:** 保障了緊急狀態傳輸的即時性，確保雲端戰情板能在第一時間收到工安危機通知。
* **Negative:** 當網路持續斷線導致 Priority Queue 滿載（`maxsize=100`）時，系統會開始無情丟棄低優先權的歷史遙測數據。在未引入本地 SQLite 緩衝機制前，將無法保證非關鍵資料的完整性（Zero Data Loss）。

## 7. Related AKB (關聯架構知識庫)
* [`AKB / 05_Gateway.md`](../AKB/05_Gateway.md)
* **Concept:** Traffic Shaping (流量塑形)、Asynchronous I/O (非同步通訊)

## 8. Evidence / References (證據與參考資料)
* 實作證據：[`it_gateway_node.py`](../../it_edge_layer/ros2_ws/src/gateways/v5_it_gateway/v5_it_gateway/it_gateway_node.py) 的 `transmit_worker` 採用了 `queue.PriorityQueue(maxsize=100)`。
* 實作證據：緊急狀態被明確賦予最高的優先級別（`prio = 0`），使其能直接插隊優先上傳。