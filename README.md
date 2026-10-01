# 五子棋 AI

原来的版本是个 Windows 控制台程序，用 `<windows.h>`、鼠标 API 和控制台绘图写的，
只能在 Windows 上跑。这次把它拆成了两块：C++ 只管算棋，Python 只管画界面，
中间走 TCP + JSON。

拆开之后 C++ 那边不再碰任何平台相关的头文件，Linux / Windows / macOS 都能编译，
界面也随时能换（比如换成一个 Web 前端），代价是多了一层进程间通信。

原来的控制台版留在根目录的 `main.cpp` 里，不参与构建，留着对照。

## 怎么跑

先编 C++ 服务端：

```bash
cd cpp
mkdir -p build && cd build
cmake ..
cmake --build . --config Release
```

产物是 `cpp/build/bin/gomoku_server`（Windows 上是 `gomoku_server.exe`）。
Windows 下用 MSVC 或 MinGW 的话换个 generator：

```bash
cmake -S . -B build -G "Visual Studio 17 2022"   # 然后 cmake --build build --config Release
cmake -S . -B build -G "MinGW Makefiles"         # 然后 cmake --build build
```

需要 CMake ≥ 3.16，以及支持 C++17 的编译器（GCC 8+ / Clang 7+ / MSVC 2019+）。

再装 Python 依赖（只有 pygame 一个，网络和 JSON 都用标准库），然后启动：

```bash
pip install -r python/requirements.txt
python python/main.py
```

启动顺序是自动的：客户端先试着连 `127.0.0.1:8888`，连不上就自己把服务端拉起来，
等端口就绪再连，退出时顺手关掉。端口上要是已经有一个服务端在跑，就直接复用不重复启动。
8888 被别的程序占着也没事，它会顺着 8889、8890 往下找，一共试 15 个。

几个常用参数：

```bash
python python/main.py --port 9000     # 换端口池的起点
python python/main.py --no-autostart  # 只连接，不自己启动服务端
python python/main.py --verbose       # 把服务端日志打到终端
```

服务端也能单独跑：

```bash
./cpp/build/bin/gomoku_server --host 127.0.0.1 --port 8888
```

## 目录结构

```
.
├── main.cpp                 原始 Windows 控制台版本（保留作对照，不参与构建）
├── cpp/
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp         服务端入口：命令行参数、信号处理、启动日志
│   │   ├── protocol.h       协议的字段名与坐标约定，外加两个小工具函数
│   │   ├── game.h/.cpp      棋盘状态机（纯逻辑，无 IO、无网络）
│   │   ├── ai.h/.cpp        AI：minimax + alpha-beta + 启发式排序
│   │   └── tcp_server.h/.cpp跨平台 TCP 服务端（winsock2 / POSIX 条件编译）
│   └── third_party/
│       └── json.hpp         nlohmann/json 3.11.3 单头文件（随仓库一起提交）
├── python/
│   ├── main.py              客户端入口：连服务端 → 开窗口
│   ├── ui.py                pygame 界面
│   ├── network.py           TCP 客户端 + 服务端子进程管理
│   ├── config.py            全部可调参数（地址、端口、尺寸、配色、字体）
│   └── requirements.txt
└── README.md
```

## 玩法

* 18×18 棋盘，黑棋先行，任意方向连成 5 子获胜，下满为平局。
* 开始界面选执黑先手还是执白后手，再选 1~4 级难度。
* 对局中点交叉点落子；右侧「悔棋」回退你和 AI 各一步，每局 3 次。
* 快捷键：`U` 悔棋，`Esc` 返回主菜单。
* 分出胜负后棋盘上会叠一层结算浮层，上面有「再来一局」和「离开」。

### 难度

| 难度 | 搜索深度 | 单步思考时间上限 |
|:--:|:--:|:--:|
| 1 级 · 入门 | 1 层 | 150 ms |
| 2 级 · 简单 | 2 层 | 500 ms |
| 3 级 · 普通 | 3 层 | 1.5 s |
| 4 级 · 困难 | 4 层 | 3 s |

AI 沿用原版的算法：minimax + alpha-beta 剪枝，评估函数也保持原来 `get_val` 的统计方式
（沿四个方向滑动长度 5 的窗口，5 连记 8×10⁸，活三 `tk==14` 额外加权），
AI 侧权重 10、玩家侧权重 80，所以 AI 偏防守。在这之上加了三点：

1. **着法排序**：按「落子后四个方向能形成的连子价值」排，原版只按邻子数排。
   排得准，alpha-beta 能剪掉的分支就多。
2. **候选点限制**：每层只展开分值最高的 14 个（根）/ 10 个（内层），
   不然深度 4 要把棋盘上所有邻接空点都展开。
3. **迭代加深 + 时间预算**：从 1 层逐层加深，把上一层的最佳着法排到最前；
   超时就丢掉没算完的那层，保留上一层结果。所以难度 4 也不会卡住界面。

另外还有两条快捷判断：能一步成五就直接取胜，对手能一步成五就直接堵。

## 通信协议

* 传输：TCP，默认 `127.0.0.1:8888`（两端都能改：`cpp/src/main.cpp`、`python/config.py`）
* 编码：UTF-8，一行一条消息，以 `\n` 分隔，消息体是紧凑的 JSON 对象

### 坐标约定（两端必须一致）

* 棋盘 18×18，合法下标 `0~17`
* **`x` 是列**（屏幕左右方向），**`y` 是行**（屏幕上下方向）
* `state` 里的 `board` 是 `board[y][x]`，即 `board[0]` 是棋盘最上面一行
* 棋盘值：`0` 空、`1` 黑、`2` 白

### 客户端 → 服务端

```jsonc
// 开新局。player=1 玩家执黑先手；player=2 玩家执白后手。difficulty 取 1~4
{"type":"new_game","player":1,"difficulty":3}

// 玩家落子
{"type":"move","x":2,"y":3}

// 悔棋：回退玩家与 AI 各一步，剩余次数减一
{"type":"undo"}

// 仅查询当前状态
{"type":"state"}
```

### 服务端 → 客户端

```jsonc
// 状态
{
  "type": "state",
  "board": [[0,0,...], ...],   // 18×18，board[y][x]
  "current_player": 1,          // 轮到谁（1 黑 / 2 白）
  "game_over": false,
  "winner": 0,                  // 0 无胜者或平局，1 黑胜，2 白胜
  "undo_left": 3,
  "message": "轮到你落子",
  "thinking": false,            // true 表示 AI 还在思考，客户端应继续等待
  // 以下为给界面用的附加字段，可以忽略
  "human_player": 1, "ai_player": 2, "difficulty": 2, "move_count": 0,
  "last_move": {"x":8,"y":8,"player":1}   // 没有落子时为 null
}

// 出错（坐标越界、位置被占、还没轮到你、悔棋次数用完……）
{"type":"error","message":"非法落子：该位置已经有棋子了"}
```

一次 `move` / `new_game` 最多会返回**两条** `state`：第一条 `thinking=true`、
`message="AI正在思考中..."`，让界面立刻把玩家那一手画出来；第二条 `thinking=false`，
是 AI 应手之后的最终状态。客户端以 `thinking` 字段决定要不要接着等。

## 一些实现上的选择

**JSON 库为什么直接塞进仓库**。用的是 nlohmann/json 3.11.3，单头文件放在
`cpp/third_party/json.hpp`，CMake 里只是加了个 include path。需求里给的方案是
CMake FetchContent，但那个要求 configure 阶段能连上 GitHub，离线机器和内网 CI
上会直接构建失败。想换成 FetchContent 的话，删掉那个头文件，在 `CMakeLists.txt` 里加上：

```cmake
include(FetchContent)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)
target_link_libraries(gomoku_server PRIVATE nlohmann_json::nlohmann_json)
```

代码里统一用 `json = nlohmann::json` 这个别名。消息里字段类型不对时 nlohmann 会抛
`json::exception`，`tcp_server.cpp` 的 `dispatch()` 用一个 try 兜成 `error` 消息，
连接不会被打断。

**服务端**。每个连接开一个 `std::thread` 并 detach，线程里各有一份独立的 `Game`，
所以多个客户端能同时开局互不干扰。主循环用带 500 ms 超时的 `select` 等新连接，
`stop()` 半秒内就能生效。`Ctrl+C` 的处理函数只置一个原子标志，不在信号上下文里加锁或做 IO。
收尾时先 `shutdown()` 再 `close()`——只有 `shutdown()` 能唤醒**另一个线程**里阻塞的
`recv()`，不然退出时会一直卡在那儿。

**客户端**。收数据在独立线程里做，解析好的消息塞进 `queue.Queue`，主线程渲染循环用
`poll()` 非阻塞地取，所以 AI 思考时界面照常刷新。断线、服务端崩溃、收到无法解析的数据，
都会包装成一条消息交给界面弹提示，而不是抛异常崩掉。点棋盘后先本地乐观落子再发消息，
界面零延迟；万一这手被服务端判非法，界面会主动拉一次 `state` 把棋盘同步回权威状态。

### 和原始版本的差异

* **平台**：原来是 Windows 控制台程序（`windows.h`），现在是纯标准库 C++ 加 pygame，三个系统都能跑。
* **输入**：原来是 `GetCursorPos` + `GetAsyncKeyState` 轮询，现在是 pygame 的鼠标事件。
* **悔棋**：原来靠一份 4 层棋盘快照数组回滚，现在服务端留着落子历史，撤掉 AI 一手再撤掉玩家一手。
* **悔棋后的轮次**：原来沿用旧变量，现在按棋子数推算（黑先、双方严格交替），更稳。
* **胜负判定**：原来全盘扫描，现在只看最后一手周围，结果等价但快得多。
* **搜索效率**：原来每层展开全部邻接空点，现在有启发式排序 + 候选点限制 + 迭代加深 + 时间预算。
* **坐标**：控制台里 `x` 是行、`y` 是列，现在统一成 `x` 是列、`y` 是行。
* **评估函数**：保持不变，还是 `get_val` 加权重 10/80，连活三 `tk==14` 的加权细节都照搬。

## 常见问题

**找不到服务端可执行文件** —— 按提示先编译 C++ 部分，或者用环境变量指定路径：

```bash
export GOMOKU_SERVER_EXE=/path/to/gomoku_server
```

**端口被占用 / 想换端口** —— 默认会自己顺着 8888 往后找，实在想指定就改
`python/config.py` 里的 `PORT`，或者启动时加 `--port`。

**界面上的汉字显示成方块** —— pygame 不带中文字体，程序会自动在系统里找
（Noto Sans CJK / 微软雅黑 / PingFang / 文泉驿等）。都没有的话装一款：

```bash
sudo apt install fonts-noto-cjk              # Debian / Ubuntu
sudo dnf install google-noto-sans-cjk-fonts   # Fedora
sudo pacman -S noto-fonts-cjk                 # Arch
```

Windows 和 macOS 系统自带中文字体，不用额外装。

**AI 在难度 4 下思考很久** —— 3 秒是设定的上限。这期间界面不会卡，会显示
「AI正在思考中…」并继续刷新。嫌慢就降难度，或者改 `cpp/src/ai.cpp` 里的 `timeBudgetMs()`。

**报 `缺少依赖：No module named 'pygame'`** —— `pip install -r python/requirements.txt`。
