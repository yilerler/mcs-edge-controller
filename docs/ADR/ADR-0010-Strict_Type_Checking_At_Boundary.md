# ADR-0010: 於閘道器邊界實施嚴格型別校驗 (Strict Type Checking)

* **ID:** ADR-0010
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `v5_it_gateway`

---

## 1. Context / Problem (背景與問題)
來自雲端面板的指令通常透過動態語言格式（如 JSON）傳輸。這種協定容易因版本不匹配、雲端開發者疏失，或是遭到惡意攻擊而產生型別混亂（例如將 PM2.5 閾值從 `float` 誤傳為惡意字串 `ILLEGAL_STRING`）。若讓這些「髒資料」直接進入系統的 DDS 內部網路，將導致後端由 C++ 撰寫的核心引擎發生解析錯誤、例外拋出（Exception）甚至崩潰。

## 2. Decision (決策內容)
在 IT 閘道器接收下行雲端指令的最初邊界，實施強硬的動態型別攔截與校驗。任何不符合預期型別或格式的負載，直接在邊界被丟棄（Drop），禁止轉發入內部網域。

## 3. Rationale (決策理據)
建立從動態語言環境 (Python/JSON/Cloud) 跨入靜態強型別系統 (C++/ROS 2) 的防衛邊界。由防腐層第一時間攔截污染源，保護內部核心大腦的記憶體空間絕對乾淨。

## 4. Alternatives (替代方案)
* **Alternative A:** 完全信任雲端發送的資料格式，將 JSON 直接轉譯並發布，由 Core Bridge 自行在接收端進行資料清理與型別轉換 (Data Sanitization)。

## 5. Rejected Alternatives (拒絕原因)
* **Rejected Alternative A because:** 髒資料不應進入內部匯流排。若交由核心引擎處理，不僅浪費了內部網路的頻寬，更增加了關鍵安全節點的運算負擔與潛在的解析漏洞攻擊面。這違反了 Gateway 作為防腐層的初衷。

## 6. Consequences (預期影響與取捨)
* **Positive:** 確保了 ROS 2 內部網域與 Core Bridge 接收到的每一份 `CloudIntent` 都是 100% 型別安全的，系統內部無需再寫任何防呆檢查。
* **Negative:** 邊界過濾變得極度無情。雲端開發端若發生微小的型別偏差（如本該傳數值 `100.0` 卻傳成字串 `"100"`），指令會被邊緣端靜默吞噬（Silently Ignored），增加雲雲對接初期的除錯溝通成本。

## 7. Related AKB (關聯架構知識庫)
* [`AKB / 05_Gateway.md`](../AKB/05_Gateway.md)
* **Concept:** Type Interception (型別防禦)、Anti-Corruption Layer (防腐層)

## 8. Evidence / References (證據與參考資料)
* 實作證據：[`it_gateway_node.py`](../../it_edge_layer/ros2_ws/src/gateways/v5_it_gateway/v5_it_gateway/it_gateway_node.py) 處理下行指令時，實作了明確且嚴格的型別過濾指令 `if not isinstance(..., (float, int)): return`。