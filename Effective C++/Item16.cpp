// roots并不改变它所操作的Polynomial对象。但是作为缓存的一部分，它也许会改变rootVals和rootsAreValid的值。
// mutable的经典使用样例
class Polynomial {
public:
    using RootsType = std::vector<double>;
    
    RootsType roots() const
    {
        if (!rootsAreValid) {               //如果缓存不可用
            …                               //计算根
                                            //用rootVals存储它们
            rootsAreValid = true;
        }
        
        return rootVals;
    }
    
private:
    mutable bool rootsAreValid{ false };    //初始化器（initializer）的
    mutable RootsType rootVals{};           //更多信息请查看条款7
};


// 对于需要同步的是单个的变量或者内存位置，使用std::atomic就足够了。
// 不过，一旦你需要对两个以上的变量或内存位置作为一个单元来操作的话，就应该使用互斥量
class Widget {
public:
    …
    int magicValue() const
    {
        std::lock_guard<std::mutex> guard(m);   //锁定m
        
        if (cacheValid) return cachedValue;
        else {
            auto val1 = expensiveComputation1();
            auto val2 = expensiveComputation2();
            cachedValue = val1 + val2;
            cacheValid = true;
            return cachedValue;
        }
    }                                           //解锁m
    …

private:
    mutable std::mutex m;
    mutable int cachedValue;                    //不再用atomic
    mutable bool cacheValid{ false };           //不再用atomic
};

/*
请记住：

确保const成员函数线程安全，除非你确定它们永远不会在并发上下文（concurrent context）中使用。
使用std::atomic变量可能比互斥量提供更好的性能，但是它只适合操作单个变量或内存位置。
*/