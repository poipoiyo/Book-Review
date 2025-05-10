在 C++ 中，default 主要有以下用途：
- 類別的函式預設值：當你希望某個函式使用預設行為時，可以使用 default 來明確指定。例如，類別的建構函式、解構函式、拷貝建構函式、移動建構函式等，可以用 default 來要求編譯器生成預設版本：
class Example {
public:
    Example() = default;  // 使用編譯器提供的預設建構函式
    ~Example() = default; // 使用編譯器提供的預設解構函式
};
- 運算子重載：在 C++11 及更新版本中，你可以使用 default 來要求編譯器提供預設的運算子行為，例如拷貝與移動運算子：
class Example {
public:
    Example(const Example&) = default; // 預設拷貝建構函式
    Example& operator=(const Example&) = default; // 預設拷貝賦值運算子
};

