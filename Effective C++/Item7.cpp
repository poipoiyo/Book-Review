int x(0);               //使用圆括号初始化

int y = 0;              //使用"="初始化

int z{ 0 };             //使用花括号初始化


Widget w1;              //调用默认构造函数

Widget w2 = w1;         //不是赋值运算，调用拷贝构造函数

w1 = w2;                //是赋值运算，调用拷贝赋值运算符（copy operator=）

// 括号初始化也能被用于为非静态数据成员指定默认初始值。C++11允许"="初始化不加花括号也拥有这种能力
class Widget{
    …

private:
    int x{ 0 };                 //没问题，x初始值为0
    int y = 0;                  //也可以
    int z(0);                   //错误！
}


// 不可拷贝的对象可以使用花括号初始化或者圆括号初始化，但是不能使用"="初始化
std::atomic<int> ai1{ 0 };      //没问题
std::atomic<int> ai2(0);        //没问题
std::atomic<int> ai3 = 0;       //错误！


// 括号表达式不允许内置类型间隐式的变窄转换
double x, y, z;
int sum1{ x + y + z };          //错误！double的和可能不能表示为int
int sum2(x + y +z);             //可以（表达式的值被截为int）
int sum3 = x + y + z;           //同上



Widget w1(10);                  //使用实参10调用Widget的一个构造函数
Widget w2();                    //最令人头疼的解析！声明一个函数w2，返回Widget
Widget w3{};                    //调用没有参数的构造函数构造对象


class Widget { 
public:  
    Widget(int i, bool b);      //构造函数未声明
    Widget(int i, double d);    //std::initializer_list这个形参 
    …
};
Widget w1(10, true);            //调用第一个构造函数
Widget w2{10, true};            //也调用第一个构造函数
Widget w3(10, 5.0);             //调用第二个构造函数
Widget w4{10, 5.0};             //也调用第二个构造函数


// 如果有一个或者多个构造函数的声明包含一个std::initializer_list形参，
// 那么使用括号初始化语法的调用更倾向于选择带std::initializer_list的那个构造函数
class Widget { 
public:  
    Widget(int i, bool b);      //同上
    Widget(int i, double d);    //同上
    Widget(std::initializer_list<long double> il);      //新添加的
    …
}; 

Widget w1(10, true);    //使用圆括号初始化，同之前一样
                        //调用第一个构造函数

Widget w2{10, true};    //使用花括号初始化，但是现在
                        //调用带std::initializer_list的构造函数
                        //(10 和 true 转化为long double)

Widget w3(10, 5.0);     //使用圆括号初始化，同之前一样
                        //调用第二个构造函数 

Widget w4{10, 5.0};     //使用花括号初始化，但是现在
                        //调用带std::initializer_list的构造函数
                        //(10 和 5.0 转化为long double)



// 甚至普通构造函数和移动构造函数都会被带std::initializer_list的构造函数劫持：

class Widget { 
public:  
    Widget(int i, bool b);                              //同之前一样
    Widget(int i, double d);                            //同之前一样
    Widget(std::initializer_list<long double> il);      //同之前一样
    operator float() const;                             //转换为float
    …
};

Widget w5(w4);                  //使用圆括号，调用拷贝构造函数

Widget w6{w4};                  //使用花括号，调用std::initializer_list构造
                                //函数（w4转换为float，float转换为double）

Widget w7(std::move(w4));       //使用圆括号，调用移动构造函数

Widget w8{std::move(w4)};       //使用花括号，调用std::initializer_list构造
                                //函数（与w6相同原因）



class Widget { 
public: 
    Widget(int i, bool b);                      //同之前一样
    Widget(int i, double d);                    //同之前一样
    Widget(std::initializer_list<bool> il);     //现在元素类型为bool
    …                                           //没有隐式转换函数
};

Widget w{10, 5.0};              //错误！要求变窄转换



class Widget { 
public:  
    Widget(int i, bool b);                              //同之前一样
    Widget(int i, double d);                            //同之前一样
    //现在std::initializer_list元素类型为std::string
    Widget(std::initializer_list<std::string> il);
    …                                                   //没有隐式转换函数
};

Widget w1(10, true);     // 使用圆括号初始化，调用第一个构造函数
Widget w2{10, true};     // 使用花括号初始化，现在调用第一个构造函数
Widget w3(10, 5.0);      // 使用圆括号初始化，调用第二个构造函数
Widget w4{10, 5.0};      // 使用花括号初始化，现在调用第二个构造函数



// 如果它们意味着没有实参，就该使用默认构造函数，但如果它意味着一个空的std::initializer_list，就该调用std::initializer_list构造函数
class Widget { 
public:  
    Widget();                                   //默认构造函数
    Widget(std::initializer_list<int> il);      //std::initializer_list构造函数

    …                                           //没有隐式转换函数
};

Widget w1;                      //调用默认构造函数
Widget w2{};                    //也调用默认构造函数
Widget w3();                    //最令人头疼的解析！声明一个函数
Widget w4({});                  //使用空花括号列表调用std::initializer_list构造函数
Widget w5{{}};                  //同上


// 最好把你的构造函数设计为不管用户是使用圆括号还是使用花括号进行初始化都不会有什么影响
std::vector<int> v1(10, 20);    //使用非std::initializer_list构造函数
                                //创建一个包含10个元素的std::vector，
                                //所有的元素的值都是20
std::vector<int> v2{10, 20};    //使用std::initializer_list构造函数
                                //创建包含两个元素的std::vector，
                                //元素的值为10和20


/*
请记住：

花括号初始化是最广泛使用的初始化语法，它防止变窄转换，并且对于C++最令人头疼的解析有天生的免疫性
在构造函数重载决议中，编译器会尽最大努力将括号初始化与std::initializer_list参数匹配，即便其他构造函数看起来是更好的选择
对于数值类型的std::vector来说使用花括号初始化和圆括号初始化会造成巨大的不同
在模板类选择使用圆括号初始化或使用花括号初始化创建对象是一个挑战。
*/
