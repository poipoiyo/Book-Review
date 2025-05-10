
// Use "const" whenever possible

const char *p = greeting; // non-const pointer, const data
char * const p = greeting; // const pointer, non-const data


const std::vector<int>::iterator iter = vec.begin(); // like T* const
*iter = 10; // ok!
++iter; // error!

std::vector<int>::const_iterator cIter = vec.begin(); // like const T*
*cIter = 10; // error!
++iter; // ok!



//在 C++ 中，const 修飾符在函數聲明中可以位於函數前或函數後，它們的作用和含義是不同的：
//1. 函數前加上 const
//如果在函數返回值的類型前加上 const，它表示函數返回的值是只讀的，不能被修改。例如：
const int getValue();


//這表示 getValue 函數的返回值不能被修改。下面是使用的例子：
const int value = getValue();
// value 是常量，不能被修改


//這種用法主要用於保護返回值，防止意外修改。
//2. 函數後加上 const
//如果在函數的括號後加上 const，表示該函數是常量函數，它不能修改屬於該類的成員變數（非 mutable 變數）。這通常用於類中的成員函數。例如：
class MyClass {
public:
    int getValue() const;
};


//此處的 getValue 是一個常量函數，它只能讀取類的成員，而不能修改它們。如果你嘗試修改某些成員變數，編譯器會報錯：
int MyClass::getValue() const {
    // this->value = 10; // 錯誤：無法在 const 函數中修改成員
    return value;
}




class CTextBlock{
public:
    std::size_t length() const;
private:
    char *pText;
    mutable std::size_t textLength;

std::size_t CTestBlock::length() const
{
    textLength = std::strlen(pText); // error!
    return textLength;
}

std::size_t CTestBlock::length() const
{
    textLength = std::strlen(pText); // ok!
    return textLength;
}
//mutable讓我們可以將特定non-static data member指定為可以修改，但仍然可以讓function pass bitwise constness的檢查
