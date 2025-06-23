template<typename T>                    //要调用的模板函数
void f(const T& param);

std::vector<Widget> createVec();        //工厂函数

const auto vw = createVec();            //使用工厂函数返回值初始化vw

if (!vw.empty()){
    f(&vw[0]);                          //调用f
    …
}

template<typename T>
void f(const T& param)
{
    using std::cout;
    cout << "T =     " << typeid(T).name() << '\n';             //显示T

    cout << "param = " << typeid(param).name() << '\n';         //显示
    …                                                           //param
}                                                               //的类型

T =     PK6Widget
param = PK6Widget

/*
请记住：

类型推断可以从IDE看出，从编译器报错看出，从Boost TypeIndex库的使用看出
这些工具可能既不准确也无帮助，所以理解C++类型推导规则才是最重要的
*/