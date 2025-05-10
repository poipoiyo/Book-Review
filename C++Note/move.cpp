/*
在 C++ 中，std::move 是 C++11 引入的一個函數模板，主要用於移動語義（Move Semantics）。它的作用是將左值轉換為右值，使得資源可以被移動，而不是拷貝，從而提高程式的效率。
std::move 的用途
- 將左值轉換為右值，允許資源被移動而非拷貝。
- 提高性能，避免不必要的深拷貝，特別是在處理大型物件時。
- 與移動構造函數和移動賦值運算符配合使用，使得物件可以高效地轉移所有權。

與 std::forward 的區別
| std::move | 無條件地將變數轉換為右值引用 | 
| std::forward | 根據模板參數決定是否轉換為右值引用 | 
*/

#include <iostream>
#include <vector>
#include <string>
#include <utility> // std::move

int main() {
    std::string str = "Hello, World!";
    std::vector<std::string> vec;

    // 使用 std::move 将字符串移动到 vector 中
    vec.push_back(std::move(str));

    std::cout << "Vector content: " << vec[0] << std::endl;
    std::cout << "String after move: " << str << std::endl; // str 的内容可能未定义，但仍是合法状态

    return 0;
}