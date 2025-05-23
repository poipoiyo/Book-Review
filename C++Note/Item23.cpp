std::move和std::forward仅仅是执行转换（cast）的函数（事实上是函数模板）。
std::move无条件的将它的实参转换为右值，而std::forward只在特定情况满足时下进行转换


std::move除了转换它的实参到右值以外什么也不做，有一些提议说它的名字叫rvalue_cast之类可能会更好

第一，不要在你希望能移动对象的时候，声明他们为const。
对const对象的移动请求会悄无声息的被转化为拷贝操作。
第二点，std::move不仅不移动任何东西，而且它也不保证它执行转换的对象可以被移动。
关于std::move，你能确保的唯一一件事就是将它应用到一个对象上，你能够得到一个右值。

/*
请记住：

std::move执行到右值的无条件的转换，但就自身而言，它不移动任何东西。
std::forward只有当它的参数被绑定到一个右值时，才将参数转换为右值。
std::move和std::forward在运行期什么也不做。
*/