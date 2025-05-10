
class MyClass {
public:
  MyClass* getPointer() {
    return this; // 返回指向當前物件的指標
  }
  MyClass& getReference() {
    return *this; // 返回當前物件的引用
  }
};