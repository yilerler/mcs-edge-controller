# ==============================================================================
# MCS Edge Controller - Global Automation Makefile
# ==============================================================================
# 警告：本腳本僅供日常開發便利使用。
# 系統底層依賴與架構約束，請務必參閱 README.md 與 docs/AKB/ 的嚴格定義。
# ==============================================================================

.PHONY: all clean ot it launch tui

# 預設執行完整「焦土重建」與部署
all: clean ot it

# ------------------------------------------------------------------------------
# 1. 焦土清理 (Scorched Earth Policy)
# ------------------------------------------------------------------------------
clean:
	@echo "🔥 [Clean] 執行焦土清理：抹除所有舊版編譯快取與快照..."
	@cd ot_defense_layer && make clean
	@rm -rf it_edge_layer/ros2_ws/build/
	@rm -rf it_edge_layer/ros2_ws/install/
	@rm -rf it_edge_layer/ros2_ws/log/
	@echo "✅ [Clean] 清理完成。"

# ------------------------------------------------------------------------------
# 2. OT 實體防禦層 (Kernel Space)
# ------------------------------------------------------------------------------
ot:
	@echo "🛡️ [OT Defense] 開始編譯並掛載 LKM 底層驅動..."
	@cd ot_defense_layer && make
	@echo "⚠️ 準備掛載 mock_elc_core.ko，鎖定 12 Bytes ABI 合約 (需要 sudo 權限)"
	@sudo insmod ot_defense_layer/src/mock_elc_core.ko || echo "模組可能已掛載，請確認。"
	@echo "✅ [OT Defense] 物理防線部署完畢。"

# ------------------------------------------------------------------------------
# 3. IT 邊緣層 (User Space ROS 2 Workspace)
# ------------------------------------------------------------------------------
it:
	@echo "🧠 [IT Edge] 步驟一：編譯內部合約訊息套件 (v5_interfaces)..."
	@cd it_edge_layer/ros2_ws && colcon build --symlink-install --packages-select v5_interfaces
	@echo "🧠 [IT Edge] 步驟二：編譯大腦、防腐層與微服務..."
	@bash -c "cd it_edge_layer/ros2_ws && source install/setup.bash && colcon build --symlink-install"
	@echo "✅ [IT Edge] ROS 2 拓樸建置完畢。"

# ------------------------------------------------------------------------------
# 4. 系統啟動與觀測 (Run & Monitor)
# ------------------------------------------------------------------------------
launch:
	@echo "🚀 [Launch] 啟動總指揮官 (Wave Launch)..."
	@bash -c "cd it_edge_layer/ros2_ws && source install/setup.bash && RCUTILS_LOGGING_MIN_SEVERITY_THRESHOLD=WARN ros2 launch v5_bringup v5_system.launch.py"

tui:
	@echo "📊 [TUI] 啟動 M6 本地戰情板 (請確保這是在獨立 TTY 執行)..."
	@bash -c "cd it_edge_layer/ros2_ws && source install/setup.bash && ros2 run m6_local_display display_node"