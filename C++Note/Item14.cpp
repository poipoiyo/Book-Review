int f(int x) throw();   //C++98风格，没有来自f的异常
int f(int x) noexcept;  //C++11风格，没有来自f的异常

// 在一个noexcept函数中，当异常可能传播到函数外时，优化器不需要保证运行时栈（the runtime stack）处于可展开状态；
// 也不需要保证当异常离开noexcept函数时，noexcept函数中的对象按照构造的反序析构。
// 而标注“throw()”异常声明的函数缺少这样的优化灵活性，没加异常声明的函数也一样。可以总结一下：

RetType function(params) noexcept;  //极尽所能优化
RetType function(params) throw();   //较少优化
RetType function(params);           //较少优化


void setup();           //函数定义另在一处
void cleanup();

void doWork() noexcept
{
 setup();               //设置要做的工作
 …                      //真实工作
 cleanup();             //执行清理动作
}
// 这里，doWork声明为noexcept，即使它调用了non-noexcept函数setup和cleanup。
// 看起来有点矛盾，其实可以猜想setup和cleanup在文档上写明了它们决不抛出异常，即使它们没有写上noexcept

/*

请记住：
noexcept是函数接口的一部分，这意味着调用者可能会依赖它
noexcept函数较之于non-noexcept函数更容易优化
noexcept对于移动语义，swap，内存释放函数和析构函数非常有用
大多数函数是异常中立的（译注：可能抛也可能不抛异常）而不是noexcept
*/
