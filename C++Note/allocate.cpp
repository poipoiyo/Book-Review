在 C++ 中，new 和 allocate 都與動態記憶體分配相關，但它們的用途和機制有所不同：
- new 運算子：
- new 是 C++ 的運算子，用於分配物件或陣列的動態記憶體。
- new 不僅分配記憶體，還會調用物件的建構函數（如果有的話）。
- 使用 new 分配的記憶體應該使用 delete 來釋放，以防止記憶體洩漏。
- 例子：
int* p = new int(10); // 分配一個整數並初始化為 10
delete p; // 釋放記憶體
- allocate（來自 std::allocator）：
- allocate 是 C++ 標準庫的 std::allocator 類提供的方法，適用於更細粒度的記憶體管理。
- allocate 僅分配原始記憶體，不會調用物件的建構函數。
- 分配的記憶體需要手動構造物件（如 std::allocator::construct），並使用 deallocate 來釋放。
- 例子：
#include <memory>

std::allocator<int> alloc;
int* p = alloc.allocate(1); // 分配原始記憶體空間
alloc.construct(p, 10); // 在記憶體中構造整數
alloc.destroy(p); // 銷毀物件
alloc.deallocate(p, 1); // 釋放記憶體


主要區別
| 特性 | new 運算子 | allocate | 
| 分配記憶體 | 是 | 是 | 
| 調用建構函數 | 是 | 否 | 
| 釋放方式 | delete | deallocate | 
| 適用範圍 | 一般物件 | 低層記憶體管理（如 STL 容器） | 

