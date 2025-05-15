在C++中，static關鍵字有不同的用途，主要用於變數、函數和類別成員，來控制作用域、存儲類型和生命周期。以下是static的主要用法：
1. 靜態變數（Static Variables）
靜態變數在函數內部或全域變數中使用，具有持久性，也就是它的值在函數多次調用間保持不變。
#include <iostream>
using namespace std;

void counter() {
    static int count = 0; // 靜態變數，只初始化一次
    count++;
    cout << "Count: " << count << endl;
}

int main() {
    counter();
    counter();
    counter();
    return 0;
}


輸出：
Count: 1
Count: 2
Count: 3


靜態變數在函數作用域內只初始化一次，但在多次函數調用時仍保留其值。
2. 靜態全域變數（Static Global Variables）
靜態全域變數的作用域限制在該文件內，不能被其他文件訪問：
static int globalVar = 42; // 只能在此文件內訪問


3. 靜態函數（Static Functions）
靜態函數的作用域僅限於該文件內：
static void helperFunction() {
    cout << "This is a static function." << endl;
}


此函數不能被其他文件引用。
4. 靜態類別成員（Static Class Members）
靜態類別成員屬於類別，而非某個特定的物件，所有物件共享同一個靜態成員：
class Example {
public:
    static int count; // 靜態成員變數
    Example() { count++; }
};

int Example::count = 0; // 初始化靜態成員

int main() {
    Example obj1, obj2, obj3;
    cout << "Total objects: " << Example::count << endl; // 輸出 3
    return 0;
}


5. 靜態類別函數（Static Member Functions）
靜態函數只能訪問靜態成員：
class Demo {
public:
    static int value;
    static void show() { // 靜態函數
        cout << "Static value: " << value << endl;
    }
};

int Demo::value = 10;

int main() {
    Demo::show(); // 直接使用類別名稱調用靜態函數
    return 0;
}


總結
- static變數在函數內僅初始化一次，但保留其值。
- static全域變數與函數的作用域受限於該文件。
- static類別成員是所有物件共享的。
- static類別函數只能訪問靜態成員。
希望這些資訊對你有所幫助！需要更深入的解釋或範例嗎？ 😃
