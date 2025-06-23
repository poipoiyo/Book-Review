std::make_unique和std::make_shared是三个make函数 中的两个：
接收任意的多参数集合，完美转发到构造函数去动态分配一个对象，然后返回这个指向这个对象的指针。第三个make函数是std::allocate_shared。
它行为和std::make_shared一样，只不过第一个参数是用来动态分配内存的allocator对象。

processWidget(std::shared_ptr<Widget>(new Widget),  //潜在的资源泄漏！
              computePriority());

所以在调用processWidget之前，必须执行以下操作，processWidget才开始执行：

1.表达式“new Widget”必须计算，例如，一个Widget对象必须在堆上被创建
2.负责管理new出来指针的std::shared_ptr<Widget>构造函数必须被执行
3.computePriority必须运行
编译器不需要按照执行顺序生成代码。

“new Widget”必须在std::shared_ptr的构造函数被调用前执行，
因为new出来的结果作为构造函数的实参，但computePriority可能在这之前，之后，或者之间执行。
也就是说，编译器可能按照这个执行顺序生成代码：
1.执行“new Widget”
2.执行computePriority
3.运行std::shared_ptr构造函数
如果按照这样生成代码，并且在运行时computePriority产生了异常，那么第一步动态分配的Widget就会泄漏。
因为它永远都不会被第三步的std::shared_ptr所管理了。

使用std::make_shared可以防止这种问题。调用代码看起来像是这样：

processWidget(std::make_shared<Widget>(),   //没有潜在的资源泄漏
              computePriority());

这是因为std::make_shared分配一块内存，同时容纳了Widget对象和控制块。
这种优化减少了程序的静态大小，因为代码只包含一个内存分配调用，并且它提高了可执行代码的速度，
因为内存只分配一次。此外，使用std::make_shared避免了对控制块中的某些簿记信息的需要，潜在地减少了程序的总内存占用。


/*
请记住：

和直接使用new相比，make函数消除了代码重复，提高了异常安全性。对于std::make_shared和std::allocate_shared，生成的代码更小更快。
不适合使用make函数的情况包括需要指定自定义删除器和希望用花括号初始化。
对于std::shared_ptrs，其他不建议使用make函数的情况包括(1)有自定义内存管理的类；(2)特别关注内存的系统，非常大的对象，以及std::weak_ptrs比对应的std::shared_ptrs活得更久。
*/