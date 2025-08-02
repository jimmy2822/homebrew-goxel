# Goxel TDD 開發流程指南

## 什麼是 TDD？

測試驅動開發 (Test-Driven Development) 是一種軟體開發方法，遵循以下循環：

1. **紅燈** - 先寫一個會失敗的測試
2. **綠燈** - 寫最少的程式碼讓測試通過
3. **重構** - 改善程式碼品質但保持測試通過

## TDD 循環步驟

### 步驟 1: 寫失敗的測試
```c
int test_voxel_create() {
    voxel_t* v = voxel_create(10, 20, 30);
    TEST_ASSERT(v != NULL, "Voxel should be created");
    TEST_ASSERT_EQ(10, v->x);
    TEST_ASSERT_EQ(20, v->y);
    TEST_ASSERT_EQ(30, v->z);
    voxel_destroy(v);
    return 1;
}
```

### 步驟 2: 實作最小功能
```c
typedef struct {
    int x, y, z;
} voxel_t;

voxel_t* voxel_create(int x, int y, int z) {
    voxel_t* v = malloc(sizeof(voxel_t));
    if (v) {
        v->x = x;
        v->y = y;
        v->z = z;
    }
    return v;
}

void voxel_destroy(voxel_t* v) {
    free(v);
}
```

### 步驟 3: 重構 (如果需要)
- 改善命名
- 抽取重複程式碼
- 優化效能
- 但保持所有測試通過！

## Goxel 專案 TDD 實踐

### 1. 新功能開發流程

```bash
# 1. 建立新的測試檔案
touch tests/tdd/test_new_feature.c

# 2. 寫測試 (會失敗)
vim tests/tdd/test_new_feature.c

# 3. 執行測試確認失敗
cd tests/tdd && make test_new_feature && ./test_new_feature

# 4. 實作功能
vim src/new_feature.c

# 5. 再次執行測試直到通過
cd tests/tdd && make test_new_feature && ./test_new_feature
```

### 2. 測試檔案結構

```c
#include "tdd_framework.h"
#include "../../src/your_module.h"

// 測試案例 1: 基本功能
int test_basic_functionality() {
    // Arrange (準備)
    
    // Act (執行)
    
    // Assert (驗證)
    TEST_ASSERT(condition, "描述預期結果");
    return 1;
}

// 測試案例 2: 邊界條件
int test_edge_cases() {
    // 測試 NULL 輸入
    // 測試空資料
    // 測試極大/極小值
    return 1;
}

// 測試案例 3: 錯誤處理
int test_error_handling() {
    // 測試錯誤輸入
    // 測試資源不足
    // 測試並發問題
    return 1;
}

int main() {
    TEST_SUITE_BEGIN();
    
    RUN_TEST(test_basic_functionality);
    RUN_TEST(test_edge_cases);
    RUN_TEST(test_error_handling);
    
    TEST_SUITE_END();
    return g_test_stats.failed_tests > 0 ? 1 : 0;
}
```

### 3. 命名規範

- 測試檔案: `test_<module_name>.c`
- 測試函數: `test_<feature>_<scenario>()`
- 清晰描述測試意圖

### 4. 測試覆蓋率目標

- 核心功能: 100% 覆蓋
- 邊界條件: 所有已知邊界
- 錯誤處理: 所有錯誤路徑
- 效能關鍵程式碼: 包含效能測試

## 實際範例：為 JSON-RPC 方法加入 TDD

### 步驟 1: 先寫測試
```c
int test_jsonrpc_create_project() {
    // 測試請求格式
    const char* request = "{"
        "\"jsonrpc\": \"2.0\","
        "\"method\": \"goxel.create_project\","
        "\"params\": [\"TestProject\", 32, 32, 32],"
        "\"id\": 1"
    "}";
    
    // 執行處理
    char* response = process_jsonrpc_request(request);
    
    // 驗證回應
    TEST_ASSERT(response != NULL, "Should get response");
    TEST_ASSERT(strstr(response, "\"result\"") != NULL, "Should have result");
    TEST_ASSERT(strstr(response, "\"error\"") == NULL, "Should not have error");
    
    free(response);
    return 1;
}
```

### 步驟 2: 實作功能直到測試通過

### 步驟 3: 加入更多測試案例
- 無效參數測試
- 並發請求測試  
- 記憶體洩漏測試

## 整合到 CI/CD

```bash
#!/bin/bash
# tests/tdd/run_all_tdd_tests.sh

echo "Running all TDD tests..."

for test in test_*; do
    if [[ -x "$test" && "$test" != *.c ]]; then
        echo "Running $test..."
        ./$test || exit 1
    fi
done

echo "All TDD tests passed!"
```

## 最佳實踐

1. **每次只專注一個測試** - 不要一次寫太多測試
2. **保持測試簡單** - 每個測試只驗證一件事
3. **測試要快** - 單元測試應該在毫秒內完成
4. **測試要獨立** - 測試之間不應該有依賴
5. **使用描述性名稱** - 測試名稱應該清楚說明測試什麼

## 常見錯誤

1. **跳過紅燈階段** - 永遠要先看到測試失敗
2. **寫太多程式碼** - 只寫讓測試通過的最少程式碼
3. **忽略重構** - 測試通過後要花時間改善程式碼
4. **測試實作細節** - 測試行為，不是實作

## 結論

TDD 能幫助我們：
- 避免浪費時間在不需要的功能
- 提早發現設計問題
- 建立可靠的回歸測試套件
- 增加程式碼品質和信心

開始使用 TDD，讓每一行程式碼都有明確的目的！