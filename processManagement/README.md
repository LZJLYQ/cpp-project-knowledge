# Process Management

这个小型 Linux C++ 示例借鉴 `app-look` 的设计思想，不复制其生产实现：

- `main()` 是组合根：创建具体依赖并注入 `ServerApp`，等待进程信号，并在退出时先关闭应用、再清理依赖。
- `ServerApp` 管理 session 生命周期。每个 session 拥有独立的 transport，同时从 `SharedResourcePool` 获取共享的上游资源。
- 资源池按 key 计数：首个 session 创建并启动资源，后续 session 复用；最后一个 session 归还后，资源才真正停止。

为了便于学习，本例在 main 线程串行执行 session 操作；`ServerApp` 和资源池不支持并发调用。真实控制服务需要增加操作串行化或合适粒度的锁。

程序启动时会创建 `viewer-a` 和 `viewer-b` 两个 session，它们共享 `camera-0`。观察日志可以看到：一个共享资源、两个独立 transport；只有两个 session 都归还引用后，共享资源才停止。

## 两条生命周期

### 进程生命周期

从头到尾阅读 `src/main.cpp`：

1. 在线程启动前屏蔽 `SIGINT`、`SIGTERM` 和 `SIGHUP`。
2. 创建 `SharedResourcePool` 和 `ConsoleSessionTransportFactory`。
3. 将两个依赖注入 `ServerApp`。
4. 用 `sigwait()` 同步等待信号。收到 `SIGHUP` 时调用 `ServerApp::reload()`，然后继续等待。
5. 收到 `SIGINT` 或 `SIGTERM` 后，调用 `ServerApp::stop()`，再调用 `SharedResourcePool::shutdown()`。

### Session 资源生命周期

跟踪 `ServerApp::startSession()` 和 `ServerApp::stopSession()`：

1. 启动 session 时，先按资源 key 获取共享资源。
2. 资源池首次遇到该 key 时创建资源；后续获取只增加引用计数。
3. transport 工厂为当前 session 创建独立 transport。
4. transport 启动失败时，归还刚获取的共享资源引用。
5. 停止 session 时，先停止它自己的 transport，再归还共享资源引用。引用归零时资源池才停止共享资源。

`SharedResourceService` 是模拟的上游工作线程，`ConsoleSessionTransport` 是模拟的 session 私有资源。实际应用中可以用采集管线和网络传输组件替换它们。

`camera-0` 是模拟的共享上游资源，`SharedResourceService` 用后台线程假装它在工作；`ConsoleSessionTransport` 则是假装每个客户端各有一个自己的传输对象。 示例没有真的打开摄像头或网络 socket ，目的是展示资源如何被获取和归还。

当前热重载只递增模拟的配置版本号，实际项目中可在这里加载并应用配置快照。

## 构建和运行

```sh
cmake -S . -B build
cmake --build build
./build/process_management
```

在另一个终端用 `kill -HUP <进程号>` 触发重载。按 `Ctrl+C` 或发送 `SIGTERM` 可优雅退出。



## Linux信号处理

```shell
# SIGINT / SIGTERM / SIGHUP
SIGINT / SIGTERM / SIGHUP 都是异步信号，用来通知进程退出 / 重新加载配置；进程默认行为可以被 signal()/ sigaction() 捕获、自定义处理。
1. SIGINT  —— 信号 2   触发来源：终端按 Ctrl + C  默认行为：终止进程。
2. SIGTERM —— 信号 15  触发来源：kill <pid>、systemd / init.d 脚本 stop、容器停止命令， 默认行为：终止进程。
3. SIGHUP —— 信号 1  触发来源：关闭 ssh 终端窗口，很多守护进程约定：收到 SIGHUP = 重载配置，不退出（nginx、sshd 经典用法）
备注：
业界约定（不是内核强制，是软件惯例）：
- SIGHUP → reload config（重载配置文件）
- SIGTERM → graceful shutdown（优雅退出）

# 当前main中信号处理方案：
这是 Linux 多线程程序标准的「信号集中处理方案：阻塞信号 + sigwait 同步等待信号」
不是用 signal () /sigaction () 注册异步信号回调！
专门解决多线程里异步信号回调一大堆坑（回调里不能跑很多函数、竞态、线程不确定哪个抓到信号）。
核心思想：
1. 主线程先把 SIGINT / SIGTERM / SIGHUP 这三个信号屏蔽（阻塞）；
2. 所有子线程都会继承这个信号掩码，所以子线程不会收到这几个信号；
3. 主线程在 sigwait() 这里同步卡住等待信号；
4. 信号来了之后，sigwait 返回，在普通上下文（不是异步信号处理器）里执行业务逻辑（reload、优雅退出）。
```



## 4.代码解读

```shell
# ServerApp 管理多个 session；每个 session 有自己的 transport，但可以共同使用资源池里的一份上游资源
main()
├── 持有 SharedResourcePool
│   └── 持有共享的 SharedResourceService（按 key 管理）
├── 持有 ConsoleSessionTransportFactory
└── 持有 ServerApp
    ├── 借用 SharedResourcePool
    ├── 借用 TransportFactory
    └── 持有多个 Session
        ├── 记录共享资源 key
        └── 独占一个 ISessionTransport
        
        
 #谁创建、谁持有？
 在 main.cpp 中，`main()` 创建资源池、transport 工厂和`ServerApp` 。它们都是局部对象，存活到`main()` 返回。
`ServerApp` 的构造函数接收资源池和工厂的引用，因此它 使用但不拥有 这两个对象。代码里的`ISharedResourcePool& resources_` 和`ISessionTransportFactory& transport_factory_` 正是这种“借用”关系。由于`main()` 先创建依赖、后创建`ServerApp` ，退出时又先停止`ServerApp` ，这些被借用的对象在应用使用它们期间都还存在。
`ServerApp` 拥有 自己的 session 表。每个 session 里的`std::unique_ptr<ISessionTransport>` 表示：transport 归这个 session 独占，session 清理时负责结束并销毁它。工厂只负责创建 transport；创建出的对象交给 session 持有。
  
        
#共享资源怎么管理？
`ServerApp` 不直接持有`SharedResourceService` ，而是把资源 key（例如`"camera-0"` ）交给资源池获取。资源池通过`std::unique_ptr<SharedResourceService>` 持有共享服务，通过引用计数记录有多少 session 正在使用它。
启动两个 session 后，状态大致是：
viewer-a：持有 transport-a，使用 camera-0
viewer-b：持有 transport-b，使用 camera-0
资源池：持有一份 camera-0 服务，引用数为 2
这里的“引用数”表示使用者数量，不是 C++ 的`shared_ptr` 引用计数。Session 通过`resource_key` 记录自己之后需要归还哪个资源；实际服务对象仍由资源池独占持有。

#生命周期怎么走？
启动 session 时， `ServerApp::startSession()` 先向资源池获取资源，再让工厂创建 transport。成功后，transport 被移动进 session 表。如果 transport 启动失败，就归还已获取的资源。
停止 session 时， `ServerApp::stopSession()` 先停止该 session 的 transport，再调用资源池`release()` ：
停止 viewer-a -> 销毁 transport-a -> camera-0 引用数 2 -> 1
停止 viewer-b -> 销毁 transport-b -> camera-0 引用数 1 -> 0 -> 停止共享服务
收到进程退出信号后，`main()` 调用`server_app.stop()` 清理所有 session，再调用`resource_pool.shutdown()` 做最终清理。对象本身离开作用域时，`ServerApp` 和资源池的析构函数也会再次清理；这些操作设计成可重复调用，作为兜底。


这份学习示例里，session 对资源的“使用权”由`acquire(key)` /`release(key)` 手动配对 ，不是自动随对象析构归还；因此需要确保每条退出路径都调用`stopSession()` 。真实系统常会把这对操作封装成一个 RAII 租约对象，让租约析构时自动归还资源，从结构上减少漏还的可能。本例也假设 session 操作串行执行，没有处理并发访问。

```

