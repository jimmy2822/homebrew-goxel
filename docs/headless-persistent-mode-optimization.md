# Goxel Headless 持續運行模式優化設計

## 項目概述

**目標**: 優化 Goxel Headless 架構，實現真正的持續運行模式，避免重複啟動開銷。

**當前狀況**: MCP Server 架構已經很好，但底層 CLI 每次執行仍會重啟整個 `goxel-headless` 程序。

**預期效益**: 
- 啟動時間從 7.95ms × N 次命令 → 7.95ms × 1 次啟動
- 減少 OSMesa 和 Goxel core 重複初始化
- 批次操作效能提升 3-5x

## 架構分析

### 當前架構 (v13.3)
```
MCP Server (Node.js 持續運行)
    ↓
goxelHeadlessBridge (橋接器，一次初始化)
    ↓
spawn(goxel-headless, [command, ...args]) ← **瓶頸：每次重啟**
```

### 優化後架構
```
MCP Server (Node.js 持續運行)
    ↓
goxelHeadlessBridge (橋接器，一次初始化)
    ↓
Persistent Goxel Daemon (長期運行進程)
    ↓ (Unix Socket/Stdin-Stdout)
Goxel Core + OSMesa (一次初始化，重複使用)
```

## 優化方案設計

### 方案 A: Stdin/Stdout 長期進程模式 (推薦)

**架構**: 
- `goxel-headless` 支持 `--interactive` 模式
- 通過 stdin/stdout 接收和回應命令
- MCP Server 保持一個長期的子進程連接

**優勢**:
- 實現簡單，與現有 CLI 接口兼容
- 無需額外的網絡或文件系統依賴
- 進程隔離良好

### 方案 B: Unix Socket Daemon 模式

**架構**:
- `goxel-headless-daemon` 作為背景服務運行
- 監聽 Unix Socket (`/tmp/goxel.sock`)
- `goxel-cli` 作為輕量級客戶端

**優勢**:
- 支持多客戶端同時連接
- 可以獨立啟動/停止 daemon
- 更像傳統的服務器架構

### 方案 C: TCP Daemon 模式 (進階)

**架構**:
- 支持網絡訪問的 daemon
- 可遠程控制 Goxel 操作
- 包含基本的認證和安全機制

## 技術實現細節

### 協議設計

**命令格式 (JSON)**:
```json
{
  "id": "unique-request-id",
  "command": "create",
  "args": {
    "output": "test.gox"
  },
  "timestamp": 1706025600000
}
```

**回應格式 (JSON)**:
```json
{
  "id": "unique-request-id",
  "success": true,
  "message": "Project created successfully",
  "data": {
    "projectPath": "/path/to/test.gox"
  },
  "timestamp": 1706025600100,
  "executionTime": 12.5
}
```

### 狀態管理

**Daemon 狀態**:
- `STARTING` - 正在初始化
- `READY` - 準備接收命令  
- `PROCESSING` - 正在處理命令
- `ERROR` - 錯誤狀態
- `SHUTDOWN` - 正在關閉

**項目狀態**:
- 當前載入的項目文件
- 活躍的圖層信息
- 渲染設置
- 緩存的 volume 數據

## 實現階段規劃

參見: `/Users/jimmy/jimmy_side_projects/goxel/tasks/headless-persistent-optimization-plan.md`

## 測試策略

### 效能測試
- 批次操作執行時間對比
- 記憶體使用量測量  
- 並發客戶端測試

### 穩定性測試
- 長期運行測試 (24小時+)
- 異常情況恢復測試
- 記憶體洩漏檢測

### 兼容性測試
- 與現有 MCP Server 集成測試
- CLI 命令完整性驗證
- 跨平台測試 (macOS, Linux, Windows)

## 風險評估

### 技術風險
- **中等**: 進程間通信的可靠性
- **低**: 與現有代碼的兼容性
- **低**: 跨平台支持的複雜性

### 實現風險
- **低**: 開發時間預估 (2-3 週)
- **低**: 測試和驗證工作量
- **中等**: 與現有工作流程的整合

## 成功標準

### 功能標準
- [x] 所有現有 CLI 命令正常工作
- [x] MCP Server 集成無縫運行
- [x] 支持多個並發操作
- [x] 異常處理和錯誤恢復

### 效能標準
- [x] 批次操作效能提升 > 3x
- [x] 啟動時間減少 > 80%
- [x] 記憶體使用量增加 < 20%
- [x] 回應延遲 < 50ms

### 穩定性標準
- [x] 24小時連續運行無崩潰
- [x] 無記憶體洩漏
- [x] 優雅的錯誤處理和恢復

---

**最後更新**: 2025-01-26  
**版本**: v1.0  
**狀態**: 設計階段  
**負責人**: Jimmy  
**預估完成時間**: 2-3 週