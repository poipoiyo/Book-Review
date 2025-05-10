1. override
- 用於明確表示子類別的函數是覆寫（override）父類別的虛函數。
- 如果函數簽名與父類別的虛函數不匹配，編譯器會報錯，避免意外的函數遮蔽（shadowing）。
- 範例：
class Base {
public:
    virtual void show() { std::cout << "Base class" << std::endl; }
};

class Derived : public Base {
public:
    void show() override { std::cout << "Derived class" << std::endl; } // 正確覆寫
};

2. final
- 用於防止類別或虛函數被進一步覆寫或繼承。
- 可以用在類別上，表示該類別不能被繼承：class FinalClass final {
class FinalClass final {
public:
    void display() { std::cout << "Final class" << std::endl; }
};

class Derived : public FinalClass { // ❌ 錯誤，FinalClass 不能被繼承
};

