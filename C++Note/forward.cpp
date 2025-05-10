/*
std::forward 的用途
- 保持參數的左值或右值屬性，避免不必要的拷貝或移動操作。
- 提高泛型函數的效率，確保參數在傳遞過程中不會被意外轉換為左值。
- 與 std::move 不同，std::forward 只在特定條件下轉換為右值，而 std::move 會無條件地將參數轉換為右值。
*/

#include <iostream>
#include <utility> // std::forward

void process(int& x) {
    std::cout << "Lvalue reference: " << x << std::endl;
}

void process(int&& x) {
    std::cout << "Rvalue reference: " << x << std::endl;
}

template <typename T>
void forward_example(T&& arg) {
    process(std::forward<T>(arg)); // 保留参数的左值或右值属性
}

int main() {
    int a = 10;
    forward_example(a);          // 输出: Lvalue reference: 10
    forward_example(20);         // 输出: Rvalue reference: 20
    forward_example(std::move(a)); // 输出: Rvalue reference: 10
    return 0;
}