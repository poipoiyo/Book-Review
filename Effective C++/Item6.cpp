std::vector<bool> features(const Widget& w);

Widget w;
…
bool highPriority = features(w)[5];     //w高优先级吗？
…
processWidget(w, highPriority);         //根据它的优先级处理w



auto highPriority = features(w)[5];     //w高优先级吗？
//情况变了。所有代码仍然可编译，但是行为不再可预测：

processWidget(w,highPriority);          //未定义行为！

auto highPriority = static_cast<bool>(features(w)[5]);

/*
记住：

不可见的代理类可能会使auto从表达式中推导出“错误的”类型
显式类型初始器惯用法强制auto推导出你想要的结果
*/