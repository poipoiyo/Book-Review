enum Color { black, white, red };   //black, white, red在
                                    //Color所在的作用域
auto white = false;                 //错误! white早已在这个作用
                                    //域中声明


enum class Color { black, white, red }; //black, white, red
                                        //限制在Color域内
auto white = false;                     //没问题，域内没有其他“white”

Color c = white;                        //错误，域中没有枚举名叫white

Color c = Color::white;                 //没问题
auto c = Color::white;                  //也没问题（也符合Item5的建议）


// Color与double比较
enum Color { black, white, red };       //未限域enum

std::vector<std::size_t>                //func返回x的质因子
  primeFactors(std::size_t x);

Color c = red;
…

if (c < 14.5) {                         // Color与double比较 (!)
    auto factors =                      // 计算一个Color的质因子(!)
      primeFactors(c);
    …
}


//现在不存在任何隐式转换可以将限域enum中的枚举名转化为任何其他类型
enum class Color { black, white, red }; //Color现在是限域enum

Color c = Color::red;                   //和之前一样，只是
...                                     //多了一个域修饰符

if (c < 14.5) {                         //错误！不能比较
                                        //Color和double
    auto factors =                      //错误！不能向参数为std::size_t
      primeFactors(c);                  //的函数传递Color参数
    …
}

if (static_cast<double>(c) < 14.5) {    //奇怪的代码，
                                        //但是有效
    auto factors =                                  //有问题，但是
      primeFactors(static_cast<std::size_t>(c));    //能通过编译
    …
}


// 可以不指定枚举名直接声明：
enum Color;         //错误！
enum class Color;   //没问题



// enum引入一个新状态值，那么可能整个系统都得重新编译
// enum class 即使Status的定义发生改变，包含这些声明的头文件也不需要重新编译
enum class Status;                  //前置声明
void continueProcessing(Status s);  //使用前置声明enum

// 至少有一种情况下非限域enum是很有用的
enum UserInfoFields { uiName, uiEmail, uiReputation };
UserInfo uInfo;                         //同之前一样
auto val = std::get<uiEmail>(uInfo);    //啊，获取用户email字段的值


enum class UserInfoFields { uiName, uiEmail, uiReputation };
UserInfo uInfo;                         //同之前一样
auto val =
    std::get<static_cast<std::size_t>(UserInfoFields::uiEmail)>
        (uInfo);


// 这可以通过std::underlying_type这个type trait获得
template<typename E>                //C++14
constexpr auto
    toUType(E enumerator) noexcept
{
    return static_cast<std::underlying_type_t<E>>(enumerator);
}
auto val = std::get<toUType(UserInfoFields::uiEmail)>(uInfo);

/*
记住

C++98的enum即非限域enum。
限域enum的枚举名仅在enum内可见。要转换为其它类型只能使用cast。
非限域/限域enum都支持底层类型说明语法，限域enum底层类型默认是int。非限域enum没有默认底层类型。
限域enum总是可以前置声明。非限域enum仅当指定它们的底层类型时才能前置。
*/
