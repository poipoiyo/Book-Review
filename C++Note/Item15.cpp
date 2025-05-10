int sz;                             //non-constexpr变量
…
constexpr auto arraySize1 = sz;     //错误！sz的值在
                                    //编译期不可知
std::array<int, sz> data1;          //错误！一样的问题
constexpr auto arraySize2 = 10;     //没问题，10是
                                    //编译期可知常量
std::array<int, arraySize2> data2;  //没问题, arraySize2是constexpr
注意const不提供constexpr所能保证之事，因为const对象不需要在编译期初始化它的值。


int sz;                            //和之前一样
…
const auto arraySize = sz;         //没问题，arraySize是sz的const复制
std::array<int, arraySize> data;   //错误，arraySize值在编译期不可知


// C++ 14
auto base = readFromDB("base");     //运行时获取这些值
auto exp = readFromDB("exponent"); 
auto baseToExp = pow(base, exp);    //运行时调用pow函数
constexpr int pow(int base, int exp) noexcept   //C++14
{
    auto result = 1;
    for (int i = 0; i < exp; ++i) result *= base;
    
    return result;
}


/*
请记住：

constexpr对象是const，它被在编译期可知的值初始化
当传递编译期可知的值时，constexpr函数可以产出编译期可知的结果
constexpr对象和函数可以使用的范围比non-constexpr对象和函数要大
constexpr是对象和函数接口的一部分
*/