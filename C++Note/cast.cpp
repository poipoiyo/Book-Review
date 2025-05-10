在 C++ 中，cast 是一種用於將一種類型轉換為另一種類型的操作。C++ 提供了多種不同的類型轉換方式，以下是主要的四種類型轉換（cast）及其用途和特點：

1. static_cast
static_cast 是最常用的類型轉換方式，用於執行顯式的類型轉換，可以在編譯時檢查類型安全。它通常用於：
- 基本類型的轉換（例如，int 到 float）。
- 指針類型之間的轉換（例如，基類指針到派生類指針）。

示例：
float f = 3.14;
int i = static_cast<int>(f); // 將浮點數轉換為整數

Base* base = new Derived();
Derived* derived = static_cast<Derived*>(base); // 基類指針轉換為派生類指針


注意：不適合進行跨類型的不安全轉換。

2. dynamic_cast
dynamic_cast 用於運行時類型檢查，主要用於指針或引用的類型轉換，且通常用於具有多態的類型（即帶有虛函數的類型）。它確保轉換是安全的，如果轉換失敗，會返回 nullptr（指針）或拋出異常（引用）。
示例：
Base* base = new Derived();
Derived* derived = dynamic_cast<Derived*>(base); // 安全轉換
if (derived) {
    // 成功，派生類的指針
} else {
    // 失敗，返回 nullptr
}


注意：如果類型不兼容，則轉換失敗。

3. const_cast
const_cast 用於移除或添加 const 修飾符，允許在需要修改常量的情況下進行操作。
示例：
const int value = 10;
int* ptr = const_cast<int*>(&value); // 移除 const 修飾符
*ptr = 20; // 修改常量（不建議）


注意：使用 const_cast 修改常量會導致未定義行為，應謹慎使用。

4. reinterpret_cast
reinterpret_cast 用於執行低級別、不安全的類型轉換。它可以轉換任意指針類型，也可以在指針和整數類型之間進行轉換。
示例：
void* ptr = malloc(sizeof(int));
int* intPtr = reinterpret_cast<int*>(ptr); // 將 void* 轉換為 int*


注意：該類型轉換非常危險，可能導致未定義行為，應僅在必要時使用。

總結
| 類型轉換 | 用途 | 安全性 | 
| static_cast | 編譯時的類型轉換，類型安全 | 高 | 
| dynamic_cast | 運行時多態類型轉換，適用於指針和引用 | 高 | 
| const_cast | 修改 const 屬性 | 中（需謹慎） | 
| reinterpret_cast | 不安全的低級類型轉換 | 低（慎用） | 



