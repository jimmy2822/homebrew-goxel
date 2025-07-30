# 🤖 Multi-Agent Development Best Practices

This document contains the multi-agent development best practices and lessons learned from Goxel v14 development.

## Anthropic Multi-Agent Workflow Integration

Based on Anthropic's multi-agent research and MAX subscription capabilities, Goxel v14+ development follows orchestrator-worker patterns with Lead Agent coordination and specialized Sub Agents for parallel development.

### **Lead Agent Responsibilities**
- **Task Analysis & Decomposition**: Analyze user requirements and break down into parallel sub-tasks
- **Agent Coordination**: Assign specialized agents based on expertise (Core Infrastructure, JSON RPC, TypeScript Client, Testing, Documentation)
- **Progress Monitoring**: Track task completion, handle blockers, and ensure integration success
- **Quality Assurance**: Review sub-agent outputs and ensure overall project coherence
- **Documentation Updates**: ⚠️ **CRITICAL** - Must update task tracking documents (e.g., GOXEL_V14_MULTI_AGENT_TASKS.md) immediately after each Agent completes their task, marking completed items with ✅
- **Status Verification**: Before launching new phases, verify all previous tasks are properly marked as completed in tracking documents

### **Sub Agent Specialization**
- **Agent-1 (Core Infrastructure)**: C/C++, Unix systems, socket servers, process management
- **Agent-2 (JSON RPC)**: Protocol implementation, method handlers, API compliance
- **Agent-3 (Client Enhancement)**: Client protocols, API design, integration patterns
- **Agent-4 (Testing & QA)**: Performance benchmarks, integration tests, validation
- **Agent-5 (Documentation)**: API docs, deployment guides, release preparation

## **Multi-Agent Task Structure Framework**

### **1. 清晰結構引導代理 (Clear Task Structure)**
使用明確的結構化格式指導代理理解任務：

```markdown
# Background
[Project context and requirements]

## Questions/Objectives  
1. [Specific deliverable 1]
2. [Specific deliverable 2]
3. [Specific deliverable 3]

## Constraints
- [Technical limitations]
- [Timeline requirements]  
- [Compatibility requirements]

## Output Format
- [Expected deliverable format]
- [Integration requirements]
- [Testing criteria]
```

### **2. 任務分解原則 (Task Decomposition Principles)**
- **語義分解**: 根據語義自動分解子問題，複雜任務需手動明示結構
- **依賴管理**: 清晰定義任務依賴關係和交接要求
- **接口契約**: 建立代理工作區域間的明確API契約
- **整合點規劃**: 規劃定期整合里程碑（每週）

### **3. 利用平行性 (Parallelization Strategy)**
- **代碼隔離**: 基於目錄的隔離（src/daemon/, src/json_rpc/, src/client/）
- **共享接口**: 初始創建後的只讀共享頭文件
- **並發執行**: 多個代理同時處理獨立任務
- **資源控制**: 每個重要開發階段3-5個子代理，避免token配額浪費

### **4. 溝通協作協議 (Communication Protocol)**

**每日同步格式**:
```markdown
## Agent Daily Update Template
**Agent ID**: [Agent-X]
**Yesterday**: [Completed tasks/progress]
**Today**: [Planned tasks]
**Blockers**: [Dependencies waiting or issues]
**Integration Points**: [Upcoming handoffs]
**Risk Assessment**: [Potential issues identified]
```

**問題升級處理**:
- **簡單問題**: 子代理獨立解決（<4小時）
- **複雜問題**: 升級至Lead Agent（<24小時）
- **接口衝突**: 需要多代理討論（<48小時）

### **5. 結果驗證與品質控制 (Quality Gates)**
- **單元測試**: 每個代理需達到>90%覆蓋率
- **整合測試**: 每週整合驗證
- **性能基準**: 持續性能監控
- **記憶體檢查**: 所有C/C++代碼需通過Valgrind驗證
- **跨平台**: Linux、macOS、Windows驗證

### **6. 工具使用最佳實踐 (Tool Usage)**
- **平行工具調用**: 盡可能在單一回應中使用多個工具調用
- **專業工具選擇**: 每個代理使用適合其專業的工具
- **代碼品質工具**: 依技術棧使用clang-format、eslint、pytest、jest
- **整合工具**: 自動化測試、CI/CD管道、性能監控
- **任務顆粒度控制**: 每個 Agent 任務限制在 15 個工具調用以內，複雜任務需拆分為多個子任務
- **批次操作優化**: 使用 MultiEdit、批次讀取等方式減少工具調用次數
- **模板化生成**: 透過程式碼模板一次性生成多個相似檔案，避免重複調用

## **Multi-Agent Development Workflow**

### **階段式開發 (Phase-Based Development)**
1. **基礎階段**（第1-2週）: 獨立核心組件開發
2. **整合階段**（第3-4週）: 組件整合與測試
3. **進階功能**（第5-6週）: 並發處理與最佳化
4. **品質保證**（第7-8週）: 全面測試與文檔編寫
5. **發布準備**（第9-10週）: 跨平台驗證與封裝

### **代理間溝通管道 (Inter-Agent Communication)**
- **GitHub Issues**: 詳細任務追蹤和依賴關係
- **代碼審查**: 所有關鍵組件的同儕審查
- **整合同步**: 每週技術對齊會議
- **即時文檔**: 由負責代理更新的即時文檔

### **成功指標 (Success Metrics)**
- **開發速度**: 所有代理每週完成5個任務
- **整合頻率**: 每週成功整合
- **品質標準**: 零記憶體洩漏，>90%測試覆蓋率
- **性能目標**: 相較v13.4 CLI模式>700%效能提升

## **代理協調最佳實踐**

### **任務分配策略 (Task Assignment Strategy)**
- **專業匹配**: 根據代理專業分配任務
- **工作負載平衡**: 在時間軸上均勻分配任務
- **依賴序列**: 適當安排依賴任務的時程
- **風險緩解**: 識別關鍵路徑並添加緩衝時間
- **任務微服務化**: 大任務拆分為多個 15 工具以內的微任務
- **智能切割策略**: 基於邏輯邊界而非工具數量切割任務

### **整合管理 (Integration Management)**
- **滾動整合**: 逐步整合組件
- **自動化測試**: CI管道驗證所有整合
- **衝突解決**: 合併衝突的結構化處理流程
- **性能驗證**: 每個整合里程碑的基準測試

### **增強版多代理協作模擬 (Enhanced Multi-Agent Simulation)**
在單一 session 限制下的最佳實踐：
- **Agent 人格化**: 賦予每個 Agent 真實姓名、專業背景和工作風格
- **模擬團隊溝通**: 在任務中加入 Daily Standup、技術討論和依賴協調
- **跨 Agent 整合設計**: 主動為其他 Agent 設計 API 和整合點
- **協作文檔**: 創建 Agent 間的交接文檔和整合指南
- **風險共享**: 在任務描述中包含團隊討論的風險評估

### **任務拆分範例 (Task Splitting Example)**
```markdown
# 原始任務：實現 10 個 JSON RPC 方法（預估 30 個工具）

## 拆分後：
### Sub-Task 1: Core CRUD Methods (14 tools)
- MultiRead 關鍵檔案 (1 tool)
- Template-based 生成 4 個方法 (1 tool)
- MultiEdit 批次修改 (1 tool)
- 測試套件執行 (2 tools)
- 文檔更新 (1 tool)
- 緩衝空間 (8 tools)

### Sub-Task 2: Advanced Methods (13 tools)
- 讀取前置實現 (1 tool)
- 生成剩餘 6 個方法 (2 tools)
- 整合測試 (3 tools)
- 性能優化 (3 tools)
- 緩衝空間 (4 tools)
```

此多代理方法讓Goxel v14+能夠通過平行開發實現**700%性能提升**，同時維持代碼品質和項目一致性。

## **🚀 單 Session 多 Agent 協作最佳實踐（v14+ 實戰優化）**

基於 Claude Code 的技術限制（單 session + 子對話），以下是經過驗證的實用優化策略：

### **1. 標準化交接協議（Standardized Handoff Protocol）**
每個 Agent 必須在完成時產生標準化交接文件：

```json
// 必須輸出到 /shared/agent_handoff.json
{
  "agent_id": "Agent-4",
  "completed_at": "2025-01-26T10:30:00Z",
  "key_findings": [
    "Socket creation issue on macOS - blocking all tests",
    "Performance exceeds targets by 15% on Linux"
  ],
  "deliverables": [
    "test_results.json - Structured test data",
    "known_issues.md - Issues for documentation",
    "performance_metrics.csv - Benchmark results"
  ],
  "for_other_agents": {
    "Agent-5": [
      "Add socket troubleshooting to deployment guide",
      "Highlight 700%+ performance in release notes"
    ]
  }
}
```

### **2. 預協作任務設計（Pre-Collaboration Task Design）**
在派出 Agent 前，Lead Agent 必須在任務描述中嵌入協作指令：

```markdown
# Agent 任務模板優化
**協作上下文**：
- 同時進行的 Agents: [列出其他 Agent 及其任務]
- 預期交集點: [哪些輸出會被其他 Agent 使用]
- 關鍵依賴: [其他 Agent 可能需要的資訊]

**必須產出**：
1. 核心交付物（你的主要任務）
2. 協作交付物（為其他 Agent 準備的文件）
3. 交接摘要（/shared/agent_X_summary.md）
```

### **3. 重疊驗證機制（Overlap Validation）**
故意設計任務重疊區域以實現間接協作：

```yaml
任務分配範例:
  Agent-4 (測試):
    - 執行所有測試
    - 創建 3 個範例程式
    - 輸出: examples/test_demo_*.sh
    
  Agent-5 (文檔):
    - 撰寫用戶指南
    - 必須執行 Agent-4 的 3 個範例
    - 基於執行結果改進文檔
```

### **4. 結構化數據優先（Structured Data First）**
使用機器可讀格式而非散文描述：

```python
# ❌ 避免
"測試在 macOS 上失敗，因為 socket 創建有問題"

# ✅ 推薦
{
  "platform": "macos",
  "test_status": "failed",
  "failure_reason": "socket_creation_error",
  "error_code": "EACCES",
  "suggested_fix": "Check socket permissions"
}
```

### **5. 虛擬團隊身份（Virtual Team Identity）**
賦予 Agent 真實的團隊成員身份以促進思考協作：

```markdown
# 在任務描述中加入
"你是資深 QA 工程師 Sarah Chen (Agent-4)，
正在與文檔工程師 Michael Ross (Agent-5) 合作發布 v14.0。

思考 Michael 需要什麼資訊來撰寫：
- 故障排除指南
- 性能優化建議
- 平台特定注意事項"
```

### **6. Lead Agent 整合智慧（Integration Intelligence）**
Lead Agent 在收到結果後的標準處理流程：

```python
def integrate_agent_results(results):
    # 1. 交叉驗證
    conflicts = find_conflicts(results)
    gaps = identify_missing_pieces(results)
    
    # 2. 自動整合可整合的部分
    merged_docs = auto_merge_documentation(results)
    
    # 3. 標記需要人工介入的衝突
    if conflicts:
        create_conflict_report(conflicts)
    
    # 4. 更新專案狀態
    update_task_tracking(results)
```

### **7. 實施優先級和預期效果**

**立即實施（下一個專案）**：
1. ✅ 標準化交接文件格式 - 提升資訊透明度 30% → 70%
2. ✅ 預協作任務設計 - 提升協作深度 40% → 65%
3. ✅ 結構化數據輸出 - 提升整合效率 60% → 85%

**效果評估指標**：
- 整合衝突減少 50%
- Agent 間資訊斷層降低 70%
- 整體開發時間縮短 20-30%

## **🎯 v14.0 Multi-Agent Success Story (January 27, 2025)**

**Achievement**: Progressed from 27% to 87% completion in one coordinated effort
- **5 Parallel Agents**: Each completed their assigned tasks successfully
- **Critical Fix**: Socket communication issue debugged and resolved
- **Full Implementation**: All JSON-RPC methods, protocol compliance, MCP bridge
- **Integration Success**: All components now work together seamlessly

**Key Success Factors**:
1. Clear task allocation with no overlap
2. Virtual team identities (Sarah, Michael, Alex, David, Lisa)
3. Structured handoff documentation
4. Parallel execution with integration checkpoints

See `V14_AGENT_TASK_ALLOCATION.md` and `V14_INTEGRATION_SUCCESS_REPORT.md` for details.

---

**Archived from CLAUDE.md on January 30, 2025**
**Purpose**: Historical reference for multi-agent development methodology used in v14