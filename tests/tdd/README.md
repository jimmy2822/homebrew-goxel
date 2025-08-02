# Goxel TDD 快速開始指南

## 為什麼使用 TDD？

根據您的需求：**避免浪費時間沒達成目標**

TDD 幫助我們：
- ✅ 明確定義目標（透過測試）
- ✅ 避免過度設計
- ✅ 快速得到回饋
- ✅ 保證程式碼品質

## 快速開始

### 1. 執行現有測試
```bash
cd tests/tdd
make
```

### 2. 建立新功能的 TDD 測試

例如：為新的 voxel 選擇功能寫測試

```c
// tests/tdd/test_voxel_selection.c
#include "tdd_framework.h"

// 第 1 步：寫會失敗的測試
int test_create_selection() {
    selection_t* sel = selection_create();
    TEST_ASSERT(sel != NULL, "Selection should be created");
    TEST_ASSERT_EQ(0, selection_count(sel));
    selection_destroy(sel);
    return 1;
}

int main() {
    TEST_SUITE_BEGIN();
    RUN_TEST(test_create_selection);
    TEST_SUITE_END();
    return g_test_stats.failed_tests > 0 ? 1 : 0;
}
```

### 3. 執行測試（會失敗）
```bash
gcc -o test_voxel_selection test_voxel_selection.c
./test_voxel_selection
# 會看到編譯錯誤或測試失敗
```

### 4. 實作最小功能
```c
// src/selection.c
typedef struct {
    int count;
} selection_t;

selection_t* selection_create() {
    return calloc(1, sizeof(selection_t));
}

int selection_count(selection_t* sel) {
    return sel ? sel->count : 0;
}

void selection_destroy(selection_t* sel) {
    free(sel);
}
```

### 5. 再次執行測試（應該通過）

## TDD 實戰範例

### 範例 1：JSON-RPC 方法
查看 `test_daemon_jsonrpc_tdd.c`
- 測試請求解析
- 測試回應序列化
- 測試錯誤處理

### 範例 2：Voxel 資料結構
查看 `example_voxel_tdd.c`
- 測試建立/刪除
- 測試添加/查詢
- 測試邊界條件

## 整合到開發流程

1. **開始新功能前**
   ```bash
   # 建立測試檔案
   touch tests/tdd/test_new_feature.c
   # 寫測試
   # 執行確認失敗
   ```

2. **開發過程中**
   ```bash
   # 執行單一測試
   cd tests/tdd
   gcc -o test_new_feature test_new_feature.c
   ./test_new_feature
   ```

3. **提交前**
   ```bash
   # 執行所有 TDD 測試
   ./tests/run_tdd_tests.sh
   ```

## 常用測試模式

### 測試初始化
```c
int test_init() {
    thing_t* thing = thing_create();
    TEST_ASSERT(thing != NULL, "Should create");
    thing_destroy(thing);
    return 1;
}
```

### 測試操作
```c
int test_operation() {
    thing_t* thing = thing_create();
    int result = thing_do_something(thing, 42);
    TEST_ASSERT_EQ(0, result);
    thing_destroy(thing);
    return 1;
}
```

### 測試錯誤處理
```c
int test_error_handling() {
    TEST_ASSERT_EQ(-1, thing_do_something(NULL, 42));
    return 1;
}
```

## 下一步

1. 為您正在開發的功能寫測試
2. 使用 TDD 循環：紅燈 → 綠燈 → 重構
3. 享受更有信心的開發過程！

記住：**每一行程式碼都應該有明確的目的**