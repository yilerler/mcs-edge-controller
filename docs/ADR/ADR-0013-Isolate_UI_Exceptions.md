# ADR-0013: 隔離 UI 渲染例外 (Isolate UI Rendering Exceptions)

* **ID:** ADR-0013
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `m6_local_display`

---

## 1. Context / Problem (背景與問題)

本地端戰情面板（M6 節點）依賴終端機介面（如 Curses）進行 UI 渲染。這類繪圖函式庫極度脆弱，當使用者隨意縮放終端機視窗尺寸，或字元編碼不支援時，極易拋出例外錯誤（Exception）導致整個 Process 崩潰。如果 UI 崩潰導致整個節點重啟，系統將連帶遺失短暫的背景資料訂閱（如日誌記錄）。

## 2. Decision (決策內容)

在 M6 顯示節點中實施「例外實體隔離」。使用 `try-except` 包覆所有的 UI 繪圖指令，遇到繪圖錯誤時直接靜默吞噬（Silently Ignored），確保例外絕對不會向上蔓延。

## 3. Rationale (決策理據)

確保應用層的環境脆弱性不會破壞背景資料鏈路的存活。UI 僅是「旁觀者（Observer）」，並非系統的關鍵安全路徑（Safety-critical Path），其崩潰不應影響資料的持續接收。

## 4. Alternatives (替代方案)

* **Alternative A:** Fail-fast (快速失敗) 策略。在遇到 UI 繪圖錯誤時，直接讓節點崩潰並拋出錯誤堆疊 (Error Stacktrace)，交由作業系統重啟節點。

## 5. Rejected Alternatives (拒絕原因)

* **Rejected Alternative A because:** 重啟節點會導致 ROS 2 必須重新建立 DDS 訂閱連線，造成暫時的資料遺失。這違反了 Observer 微服務的優雅降級原則（即使看不見畫面，背景也應該繼續默默收資料）。

## 6. Consequences (預期影響與取捨)

* **Positive:** 獲得了極高的節點存活率。即使終端機視窗被縮小到無法繪圖，背景的訂閱與日誌記錄依然穩定運作，阻斷了 Crash 的蔓延。
* **Negative:** 隱藏了開發階段的錯誤（Error Masking）。當真的發生 UI 邏輯 Bug 時，程式不會拋出任何警告，使用者只會看到畫面卡住或破圖，增加除錯的困難度。

## 7. Related AKB (關聯架構知識庫)

* [`AKB / 06_Service.md`](../AKB/06_Service.md)
* **Concept:** UI Crash Isolation (UI 崩潰隔離)、Observer Pattern (旁觀者模式)

## 8. Evidence / References (證據與參考資料)

* 實作證據：[`display_node.py`](../../it_edge_layer/ros2_ws/src/services/m6_local_display/m6_local_display/display_node.py) 的 Curses 繪圖迴圈中，明確使用了 `try-except curses.error: pass` 包覆繪圖區塊。