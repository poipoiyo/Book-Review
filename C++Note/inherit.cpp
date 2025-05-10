在 C++ 中，有三種繼承方式：公有繼承 (public inheritance)、保護繼承 (protected inheritance) 和私有繼承 (private inheritance)。它們的主要區別在於基類成員在派生類中的可見性和訪問權限。

1. 公有繼承 (public inheritance)
特性
- 基類的公有成員 (public) 仍然是公有的，可以在派生類外部直接訪問。
- 基類的保護成員 (protected) 仍然是保護的，只能在派生類或其子類內部訪問。
- 基類的私有成員 (private) 無法直接訪問，但仍然存在於記憶體結構中。

範例
#include <iostream>
using namespace std;

class Base {
public:
    void publicMethod() { cout << "Base::publicMethod" << endl; }
protected:
    void protectedMethod() { cout << "Base::protectedMethod" << endl; }
private:
    void privateMethod() { cout << "Base::privateMethod" << endl; } // 無法在派生類中訪問
};

class Derived : public Base {
public:
    void useBaseMethods() {
        publicMethod();     // ✅ 仍然是 public
        protectedMethod();  // ✅ 仍然是 protected
        // privateMethod(); // ❌ 無法訪問
    }
};

int main() {
    Derived d;
    d.publicMethod();  // ✅ 可直接使用
    // d.protectedMethod(); // ❌ 無法直接使用
    return 0;
}


用途
適用於「is-a」關係，如「狗 is-a 動物」，並允許透過多態性操作基類函數。

2. 保護繼承 (protected inheritance)
特性
- 基類的公有成員 (public) 變為保護成員 (protected)，僅能在派生類及其子類內部訪問。
- 基類的保護成員 (protected) 保持不變。
- 基類的私有成員 (private) 仍然無法訪問。

範例
class Derived : protected Base {
public:
    void useBaseMethods() {
        publicMethod();     // ✅ 變為 protected
        protectedMethod();  // ✅ 仍然是 protected
    }
};

int main() {
    Derived d;
    // d.publicMethod(); // ❌ 無法直接使用，因為它已變為 protected
    return 0;
}


用途
適用於「類別內部需要基類功能，但不希望公開接口」，例如「員工 is-a 公司成員」。

3. 私有繼承 (private inheritance)
特性
- 基類的公有成員 (public) 變為私有成員 (private)，無法在派生類外部訪問。
- 基類的保護成員 (protected) 變為私有成員 (private)，僅能在派生類內部使用。
- 基類的私有成員 (private) 仍然無法訪問。

範例
class Derived : private Base {
public:
    void useBaseMethods() {
        publicMethod();     // ✅ 變為 private，僅可在此類內部使用
        protectedMethod();  // ✅ 變為 private，僅可在此類內部使用
    }
};

int main() {
    Derived d;
    // d.publicMethod(); // ❌ 無法使用
    return 0;
}


用途
適用於「使用基類功能但不希望暴露繼承關係」，例如「汽車 is-implemented-in 車輛技術」。