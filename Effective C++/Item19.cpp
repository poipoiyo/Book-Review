当最后一个指向某对象的std::shared_ptr不再指向那
（比如因为std::shared_ptr被销毁或者指向另一个不同的对象），std::shared_ptr会销毁它所指向的对象。

std::shared_ptr大小是原始指针的两倍，
因为它内部包含一个指向资源的原始指针，还包含一个指向资源的引用计数值的原始指针。

引用计数的内存必须动态分配。 概念上，引用计数与所指对象关联起来，但是实际上被指向的对象不知道这件事情
（译注：不知道有一个关联到自己的计数值）。因此它们没有办法存放一个引用计数值。

递增递减引用计数必须是原子性的，因为多个reader、writer可能在不同的线程。
原子操作通常比非原子操作要慢，所以即使引用计数通常只有一个word大小，你也应该假定读写它们是存在开销的。

从另一个std::shared_ptr移动构造新std::shared_ptr会将原来的std::shared_ptr设置为null，
那意味着老的std::shared_ptr不再指向资源，同时新的std::shared_ptr指向资源。
这样的结果就是不需要修改引用计数值。因此移动std::shared_ptr会比拷贝它要快：
拷贝要求递增引用计数值，移动不需要。
移动赋值运算符同理，所以移动构造比拷贝构造快，移动赋值运算符也比拷贝赋值运算符快。


对于std::unique_ptr来说，删除器类型是智能指针类型的一部分。对于std::shared_ptr则不是：
auto loggingDel = [](Widget *pw)        //自定义删除器
                  {                     //（和条款18一样）
                      makeLogEntry(pw);
                      delete pw;
                  };

std::unique_ptr<                        //删除器类型是
    Widget, decltype(loggingDel)        //指针类型的一部分
    > upw(new Widget, loggingDel);
std::shared_ptr<Widget>                 //删除器类型不是
    spw(new Widget, loggingDel);        //指针类型的一部分



std::shared_ptr的设计更为灵活。
考虑有两个std::shared_ptr<Widget>，每个自带不同的删除器（比如通过lambda表达式自定义删除器）：
auto customDeleter1 = [](Widget *pw) { … };     //自定义删除器，
auto customDeleter2 = [](Widget *pw) { … };     //每种类型不同
std::shared_ptr<Widget> pw1(new Widget, customDeleter1);
std::shared_ptr<Widget> pw2(new Widget, customDeleter2);

因为pw1和pw2有相同的类型，所以它们都可以放到存放那个类型的对象的容器中：

std::vector<std::shared_ptr<Widget>> vpw{ pw1, pw2 };
它们也能相互赋值，也可以传入一个形参为std::shared_ptr<Widget>的函数。
但是自定义删除器类型不同的std::unique_ptr就不行，因为std::unique_ptr把删除器视作类型的一部分。


另一个不同于std::unique_ptr的地方是，指定自定义删除器不会改变std::shared_ptr对象的大小。
它必须使用更多的内存。然而，那部分内存不是std::shared_ptr对象的一部分。那部分在堆上面
因为引用计数是另一个更大的数据结构的一部分，那个数据结构通常叫做控制块（control block）。
每个std::shared_ptr管理的对象都有个相应的控制块。
控制块除了包含引用计数值外还有一个自定义删除器的拷贝，当然前提是存在自定义删除器。
如果用户还指定了自定义分配器，控制块也会包含一个分配器的拷贝


std::make_shared总是创建一个控制块

当从独占指针（即std::unique_ptr或者std::auto_ptr）上构造出std::shared_ptr时会创建控制块。
独占指针没有使用控制块，所以指针指向的对象没有关联控制块。

当从原始指针上构造出std::shared_ptr时会创建控制块。
这些规则造成的后果就是从原始指针上构造超过一个std::shared_ptr就会让你走上未定义行为的快车道，
因为指向的对象有多个控制块关联。多个控制块意味着多个引用计数值，多个引用计数值意味着对象将会被销毁多次
（每个引用计数一次）。

auto pw = new Widget;                           //pw是原始指针
…
std::shared_ptr<Widget> spw1(pw, loggingDel);   //为*pw创建控制块
…
std::shared_ptr<Widget> spw2(pw, loggingDel);   //为*pw创建第二个控制块

这种情况下，指向的对象是*pw（即pw指向的对象）。
就其本身而言没什么问题，但是将同样的原始指针传递给spw2的构造函数会再次为*pw创建一个控制块
（所以也有个引用计数值）。因此*pw有两个引用计数值，每一个最后都会变成零，然后最终导致*pw销毁两次。
第二个销毁会产生未定义行为。


如果上面代码第一部分这样重写：
std::shared_ptr<Widget> spw1(new Widget,    //直接使用new的结果
                             loggingDel);
std::shared_ptr<Widget> spw2(spw1);         //spw2使用spw1一样的控制块



std::vector<std::shared_ptr<Widget>> processedWidgets;
继续，假设Widget有一个用于处理的成员函数：

class Widget {
public:
    …
    void process();
    …
};

void Widget::process()
{
    …                                       //处理Widget
    processedWidgets.emplace_back(this);    //然后将它加到已处理过的Widget
}                                           //的列表中，这是错的！
错误的部分是传递this
上面的代码可以通过编译，但是向std::shared_ptr的容器传递一个原始指针（this），
std::shared_ptr会由此为指向的Widget（*this）创建一个控制块。
那看起来没什么问题，直到你意识到如果成员函数外面早已存在指向那个Widget对象的指针
避免创建过多控制块

std::shared_ptrAPI已有处理这种情况的设施。std::enable_shared_from_this
class Widget: public std::enable_shared_from_this<Widget> {
public:
    …
    void process();
    …
};

void Widget::process()
{
    //和之前一样，处理Widget
    …
    //把指向当前对象的std::shared_ptr加入processedWidgets
    processedWidgets.emplace_back(shared_from_this());
}


std::shared_ptr不能处理的另一个东西是数组。
和std::unique_ptr不同的是，std::shared_ptr的API设计之初就是针对单个对象的，没有办法std::shared_ptr<T[]>。

/*
请记住：

std::shared_ptr为有共享所有权的任意资源提供一种自动垃圾回收的便捷方式。
较之于std::unique_ptr，std::shared_ptr对象通常大两倍，控制块会产生开销，需要原子性的引用计数修改操作。
默认资源销毁是通过delete，但是也支持自定义删除器。删除器的类型是什么对于std::shared_ptr的类型没有影响。
避免从原始指针变量上创建std::shared_ptr。
*/

