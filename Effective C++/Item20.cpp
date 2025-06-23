一个类似std::shared_ptr但不影响对象引用计数的指针。
这种类型的智能指针必须要解决一个std::shared_ptr不存在的问题：可能指向已经销毁的对象。
一个真正的智能指针应该跟踪所指对象，在悬空时知晓，悬空（dangle）就是指针指向的对象不再存在。
这就是对std::weak_ptr最精确的描述。

当从std::shared_ptr上创建std::weak_ptr时两者指向相同的对象，但是std::weak_ptr不会影响所指对象的引用计数：
auto spw =                      //spw创建之后，指向的Widget的
    std::make_shared<Widget>(); //引用计数（ref count，RC）为1。
                                //std::make_shared的信息参见条款21
…
std::weak_ptr<Widget> wpw(spw); //wpw指向与spw所指相同的Widget。RC仍为1
…
spw = nullptr;                  //RC变为0，Widget被销毁。
                                //wpw现在悬空

悬空的std::weak_ptr被称作已经expired（过期）。
if (wpw.expired()) …            //如果wpw没有指向对象…

std::shared_ptr<Widget> spw1 = wpw.lock();  //如果wpw过期，spw1就为空

std::shared_ptr<Widget> spw3(wpw);          //如果wpw过期，抛出std::bad_weak_ptr异常


std::unique_ptr<const Widget> loadWidget(WidgetID id);
如果调用loadWidget是一个昂贵的操作



std::shared_ptr<const Widget> fastLoadWidget(WidgetID id)
{
    static std::unordered_map<WidgetID,
                              std::weak_ptr<const Widget>> cache;
                                        //译者注：这里std::weak_ptr<const Widget>是高亮
    auto objPtr = cache[id].lock();     //objPtr是去缓存对象的
                                        //std::shared_ptr（或
                                        //当对象不在缓存中时为null）

    if (!objPtr) {                      //如果不在缓存中
        objPtr = loadWidget(id);        //加载它
        cache[id] = objPtr;             //缓存它
    }
    return objPtr;
}

fastLoadWidget的实现忽略了以下事实：缓存可能会累积过期的std::weak_ptr，这些指针对应了不再使用的Widget

/*
请记住：

用std::weak_ptr替代可能会悬空的std::shared_ptr。
std::weak_ptr的潜在使用场景包括：缓存、观察者列表、打破std::shared_ptr环状结构。
*/

