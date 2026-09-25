# 五子棋 AI · 跨平台重构版

把原来的 Windows 控制台五子棋（`<windows.h>` + 鼠标 API + 控制台绘制）重构为
**计算与界面分离**的三层结构：

| 层 | 技术 | 职责 |
|---|---|---|
| 计算模块 | C++17（纯标准库） | 棋盘状态、胜负判定、悔棋、minimax + alpha-beta AI |
| 通信层 | TCP + JSON 行协议 | 两端约定好的一行一条 UTF-8 JSON 消息 |
| 界面模块 | Python 3 + pygame | 开始界面、棋盘绘制、鼠标交互、状态显示 |

这样拆分之后：C++ 侧不再包含任何平台特定头文件，Linux / Windows / macOS 都能编译；
界面换成跨平台的 pygame；两者只通过一个进程间协议耦合，任意一侧都可以单独替换
（比如把 pygame 界面换成 Web 前端）。

---

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

---

## 快速开始

### 1. 编译 C++ 服务端

```bash
cd cpp
mkdir -p build && cd build
cmake ..
cmake --build . --config Release
```

产物：`cpp/build/bin/gomoku_server`（Windows 下为 `gomoku_server.exe`）。

* Windows + MSVC：`cmake -S . -B build -G "Visual Studio 17 2022"` 然后
  `cmake --build build --config Release`
* Windows + MinGW：`cmake -S . -B build -G "MinGW Makefiles"` 然后 `cmake --build build`
* 需要 CMake ≥ 3.16、支持 C++17 的编译器（GCC 8+ / Clang 7+ / MSVC 2019+）

### 2. 安装 Python 依赖

```bash
pip install -r python/requirements.txt
```

只有 pygame 一个第三方依赖，网络与 JSON 都用标准库。

### 3. 运行

```bash
python python/main.py
```

启动顺序是自动的：客户端先尝试连接 `127.0.0.1:8888`，连不上就自己
`subprocess.Popen` 拉起编译好的服务端，等端口就绪后连上；退出时会关掉由它启动的
服务端。如果端口上已经有一个服务端在跑，就直接复用，不重复启动。

常用参数：

```bash
python python/main.py --port 9000     # 换端口
python python/main.py --no-autostart  # 只连接，不自动启动服务端
python python/main.py --verbose       # 把服务端日志打到终端，排查问题用
```

也可以单独手动跑服务端：

```bash
./cpp/build/bin/gomoku_server --host 127.0.0.1 --port 8888
```

---

## 玩法

* 18×18 棋盘，黑棋先行，任意方向连成 5 子获胜，棋盘下满为平局。
* 开始界面选**执黑先手**或**执白后手**，再选难度 1~4 级。
* 对局中点棋盘交叉点落子；点右侧「悔棋」回退**你和 AI 各一步**，每局 3 次。
* 快捷键：`U` 悔棋，`Esc` 返回主菜单。
* 胜负揭晓后在棋盘上叠加结算浮层，提供「再来一局」和「离开」。

### 难度与 AI

| 难度 | 搜索深度 | 单步思考时间上限 |
|:--:|:--:|:--:|
| 1 级 · 入门 | 1 层 | 150 ms |
| 2 级 · 简单 | 2 层 | 500 ms |
| 3 级 · 普通 | 3 层 | 1.5 s |
| 4 级 · 困难 | 4 层 | 3 s |

AI 沿用原始版本的算法：**minimax + alpha-beta 剪枝**，评估函数保持原 `get_val`
的“沿四个方向滑动长度 5 的窗口”统计方式（含 5 连 8×10⁸、活三 `tk==14` 额外加权等
细节），AI 侧权重 10、玩家侧权重 80，因此 AI 偏重防守。在此之上做了三点工程化改进：

1. **着法排序**：用“落子后在四个方向能形成的连子价值”排序（原版只按邻子数排序），
   配合 alpha-beta 能剪掉大量分支。
2. **候选点限制**：每层只展开分值最高的 14 个（根）/ 10 个（内层）候选点，
   避免深度 4 时展开棋盘上所有邻接空点。
3. **迭代加深 + 时间预算**：从 1 层逐层加深，把上一层的最佳着法排到最前，
   超时则丢弃未完成的那一层、保留上一层结果。所以即使难度 4 也不会卡住界面，
   而且高难度下若已经算到必胜会提前结束。

另外还有两条快捷判断：能一步成五就直接取胜，对手能一步成五就直接堵住。

---

## 通信协议

* 传输：TCP，`127.0.0.1:8888`（可在 `cpp/src/main.cpp` 与 `python/config.py` 中改）
* 编码：UTF-8
* 分帧：**一行一条消息**，以 `\n` 分隔，消息体是紧凑的 JSON 对象

### 坐标约定（两端必须一致）

* 棋盘 18×18，合法下标 `0~17`
* **`x` 是列**（屏幕左右方向），**`y` 是行**（屏幕上下方向）
* `state` 消息里的 `board` 是 `board[y][x]`，即 `board[0]` 是棋盘最上面一行
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

> **注意**：一次 `move` / `new_game` 最多会返回**两条** `state`。
> 第一条 `thinking=true`、`message="AI正在思考中..."`，让界面立刻画出玩家那一手；
> 第二条 `thinking=false`，是 AI 应手之后的最终状态。客户端以 `thinking` 字段
> 决定要不要继续等。

---

## 设计说明

### 关于 JSON 库

用的是 nlohmann/json 3.11.3，单头文件直接放在 `cpp/third_party/json.hpp` 并随仓库提交，
CMake 里只是把它加进 include path。需求里还给了 CMake FetchContent 的方案，但那个要求
configure 阶段能访问 GitHub——离线机器、内网 CI 上整个项目会直接构建失败，所以默认走
随仓库自带的方式：`git clone && cmake ..` 一次通过，零外部依赖。

想换成 FetchContent 的话，把 `third_party/json.hpp` 删掉，在 `CMakeLists.txt` 里加上：

```cmake
include(FetchContent)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)
target_link_libraries(gomoku_server PRIVATE nlohmann_json::nlohmann_json)
```

代码里统一用 `json = nlohmann::json` 这个别名和它的常用接口（`value()`、`contains()`、
`dump()`、隐式构造）。消息里字段类型不对时 nlohmann 会抛 `json::exception`，
`tcp_server.cpp` 的 `dispatch()` 用一个 try 把它兜成 `error` 消息，连接不会被打断。

### 服务端的并发与退出

* 每个连接开一个 `std::thread`（detach 掉），线程里各有一份独立的 `Game`，
  所以多个客户端可以同时开局互不干扰。
* 主循环用带 500 ms 超时的 `select` 等连接，`stop()` 半秒内就能生效。
* `Ctrl+C` 的处理函数只置一个原子标志（`requestStop()`），不在信号上下文里加锁或者做 IO。
* 收尾时先 `shutdown()` 再 `close()`：只有 `shutdown()` 能唤醒**另一个线程**里阻塞的
  `recv()`，否则退出时会一直卡在那儿。
* `stop()` 靠一个计数器 + 条件变量等所有连接线程退出，等的时候锁是放开的，
  不会跟工作线程抢同一把锁。

### 客户端（Python）的结构

* 接收在独立线程里完成，解析好的消息塞进 `queue.Queue`，主线程渲染循环用
  `poll()` 非阻塞地取——所以 AI 思考时界面照常刷新、动画继续。
* 断线、服务端崩溃、收到无法解析的数据，都会被包装成一条消息交给界面处理，
  弹出「连接已断开」提示而不是抛异常崩掉。
* 点棋盘后先本地乐观落子再发消息，界面零延迟响应；若服务端判定这一步非法，
  界面会自动向服务端拉一次 `state` 把棋盘同步回权威状态。

### 与原始版本的行为差异

| 项目 | 原始版本 | 本版本 |
|---|---|---|
| 平台 | Windows 控制台（`windows.h`） | C++ 纯标准库 + pygame，三平台通用 |
| 输入 | `GetCursorPos` + `GetAsyncKeyState` 轮询 | pygame 鼠标事件 |
| 悔棋 | 靠一份 4 层棋盘快照数组回滚 | 服务端维护落子历史，按“撤掉 AI 的落子 + 撤掉玩家一手”回退 |
| 悔棋后的轮次 | 沿用旧变量 | 按棋子数推算（黑先、双方严格交替），更稳 |
| 胜负判定 | 全盘扫描 | 只检查最后一手周围，等价且更快 |
| 搜索效率 | 每层展开全部邻接空点 | 启发式排序 + 候选点限制 + 迭代加深 + 时间预算 |
| 坐标 | 控制台里 `x` 是行、`y` 是列 | 统一为 `x` 是列、`y` 是行（见协议说明） |
| 评估函数 | `get_val` + 权重 10/80 | **保持不变**（含活三 `tk==14` 的加权细节） |

---

## 常见问题

**找不到服务端可执行文件**
按提示先编译 C++ 部分，或用环境变量指定路径：

```bash
export GOMOKU_SERVER_EXE=/path/to/gomoku_server
```

**端口被占用 / 想换端口**
改 `python/config.py` 里的 `PORT`，或启动时加 `--port`。

**界面上的汉字显示成方块**
pygame 不带中文字体，程序会自动在系统里查找（Noto Sans CJK / 微软雅黑 / PingFang /
文泉驿等）。都没有的话装一款即可：

```bash
sudo apt install fonts-noto-cjk        # Debian / Ubuntu
sudo dnf install google-noto-sans-cjk-fonts   # Fedora
sudo pacman -S noto-fonts-cjk          # Arch
```

Windows 与 macOS 系统自带中文字体，无需额外安装。

**AI 在难度 4 下思考很久**
这是预期行为（时间上限 3 秒）。界面在此期间不会卡住，会显示「AI正在思考中…」并
继续刷新动画。想让它快一点就降低难度，或改 `cpp/src/ai.cpp` 里的 `timeBudgetMs()`。

**终端里报“缺少依赖：No module named 'pygame'”**
`pip install -r python/requirements.txt`。
