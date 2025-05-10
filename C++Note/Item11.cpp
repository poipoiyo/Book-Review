//用“= delete”将拷贝构造函数和拷贝赋值运算符标记为deleted函数
// deleted函数不能以任何方式被调用，即使你在成员函数或者友元函数里面调用deleted函数也不能通过编译。
template <class charT, class traits = char_traits<charT> >
class basic_ios : public ios_base {
public:
    …

    basic_ios(const basic_ios& ) = delete;
    basic_ios& operator=(const basic_ios&) = delete;
    …
};

// 如果要将老代码的“私有且未定义”函数替换为deleted函数时请一并修改它的访问性为public，这样可以让编译器产生更好的错误信息。



//C++有沉重的C包袱，使得含糊的、能被视作数值的任何类型都能隐式转换为int，但是有一些调用可能是没有意义的：

if (isLucky('a')) …         //字符'a'是幸运数？
if (isLucky(true)) …        //"true"是?
if (isLucky(3.5)) …         //难道判断它的幸运之前还要先截尾成3？
// 如果幸运数必须真的是整型，我们该禁止这些调用通过编译。

// 其中一种方法就是创建deleted重载函数，其参数就是我们想要过滤的类型：

bool isLucky(int number);       //原始版本
bool isLucky(char) = delete;    //拒绝char
bool isLucky(bool) = delete;    //拒绝bool
bool isLucky(double) = delete;  //拒绝float和double



// 如果processPointer是类Widget里面的模板函数， 你想禁止它接受void*参数
class Widget {
public:
    …
    template<typename T>
    void processPointer(T* ptr)
    { … }
    …

};

template<>                                          //还是public，
void Widget::processPointer<void>(void*) = delete;  //但是已经被删除了


/*
请记住：

比起声明函数为private但不定义，使用deleted函数更好
任何函数都能被删除（be deleted），包括非成员函数和模板实例（译注：实例化的函数）
*/


