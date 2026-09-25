"""全局配置：网络、棋盘规则、界面尺寸、配色、字体和服务端可执行文件的查找。

路径一律用 os.path 拼，Windows / Linux / macOS 通用。
"""

import os
import sys

# 环境变量优先，方便手动指定：export GOMOKU_SERVER_EXE=/path/to/gomoku_server
ENV_SERVER_EXE = "GOMOKU_SERVER_EXE"

PYTHON_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(PYTHON_DIR)
CPP_DIR = os.path.join(PROJECT_ROOT, "cpp")

IS_WINDOWS = os.name == "nt"
SERVER_EXE_NAME = "gomoku_server.exe" if IS_WINDOWS else "gomoku_server"

# 默认值要和 cpp/src/main.cpp 保持一致
HOST = "127.0.0.1"
PORT = 8888

AUTO_START_SERVER = True      # 连不上时自动拉起 C++ 服务端
SERVER_START_TIMEOUT = 15.0   # 等端口就绪的上限（秒）
CONNECT_TIMEOUT = 5.0
RECV_TIMEOUT = None           # 接收线程一直阻塞等，不设超时

BOARD_SIZE = 18
WIN_LENGTH = 5
UNDO_LIMIT = 3
MIN_DIFFICULTY = 1
MAX_DIFFICULTY = 4

DIFFICULTY_LABELS = {
    1: "1 级 · 入门",
    2: "2 级 · 简单",
    3: "3 级 · 普通",
    4: "4 级 · 困难",
}

CELL = 34                              # 相邻交叉点的间距
BOARD_ORIGIN = (72, 92)                # 左上角交叉点在窗口里的位置
BOARD_PIXELS = CELL * (BOARD_SIZE - 1)
BOARD_PAD = int(CELL * 0.85)           # 网格到木色底板边缘的留白

BOARD_AREA_W = BOARD_ORIGIN[0] * 2 + BOARD_PIXELS
PANEL_X = BOARD_AREA_W
PANEL_W = 320
WINDOW_W = PANEL_X + PANEL_W
WINDOW_H = BOARD_ORIGIN[1] + BOARD_PIXELS + 72

FPS = 60
STONE_RADIUS = int(CELL * 0.44)

# 18×18 没有官方星位，这几个纯粹为了好看
STAR_POINTS = [(3, 3), (3, 14), (14, 3), (14, 14), (8, 8)]

COLOR_WINDOW_BG = (32, 34, 40)
COLOR_PANEL_BG = (44, 47, 56)
COLOR_PANEL_LINE = (62, 66, 78)
COLOR_BOARD_BG = (226, 190, 140)
COLOR_BOARD_SHADOW = (196, 160, 112)   # 棋子投影，画在木色底板上
COLOR_BOARD_EDGE = (150, 116, 72)
COLOR_GRID = (120, 88, 50)
COLOR_STAR = (96, 70, 40)

COLOR_STONE_BLACK = (24, 24, 28)
COLOR_STONE_BLACK_EDGE = (10, 10, 12)
COLOR_STONE_WHITE = (248, 248, 246)
COLOR_STONE_WHITE_EDGE = (176, 176, 172)

COLOR_TEXT = (238, 238, 240)
COLOR_TEXT_DIM = (150, 154, 164)
COLOR_ACCENT = (86, 170, 255)
COLOR_ACCENT_HOVER = (120, 192, 255)
COLOR_SUCCESS = (110, 210, 140)
COLOR_DANGER = (232, 106, 106)
COLOR_WARNING = (240, 190, 90)

COLOR_BUTTON = (60, 65, 78)
COLOR_BUTTON_HOVER = (78, 84, 100)
COLOR_BUTTON_DISABLED = (48, 51, 60)
COLOR_BUTTON_SELECTED = (52, 108, 176)
COLOR_LAST_MOVE = (232, 90, 90)

# 各平台常见的中文字体，先命中先用
CJK_FONT_CANDIDATES = [
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc",
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/winfonts/NotoSansSC-VF.ttf",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
    "/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc",
    "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
    "/usr/share/fonts/truetype/arphic/uming.ttc",
    "/System/Library/Fonts/PingFang.ttc",
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/Library/Fonts/Arial Unicode.ttf",
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/msyh.ttf",
    "C:/Windows/Fonts/simhei.ttf",
    "C:/Windows/Fonts/simsun.ttc",
]

# 交给 pygame.font.match_font 的名字
CJK_FONT_NAMES = (
    "notosanscjksc,notosanscjk,notosanssc,sourcehansanssc,"
    "wenquanyimicrohei,wenquanyizenhei,droidsansfallback,"
    "microsoftyahei,simhei,simsun,pingfangsc,hiraginosansgb,arialunicode"
)


def find_cjk_font():
    """返回可用的中文字体路径，找不到就返回 None（汉字会显示成方块）。"""
    for path in CJK_FONT_CANDIDATES:
        if os.path.isfile(path):
            return path

    try:
        import pygame.font

        matched = pygame.font.match_font(CJK_FONT_NAMES)
        if matched:
            return matched
    except Exception:
        pass

    if not IS_WINDOWS:
        try:
            import subprocess

            out = subprocess.run(["fc-match", "-f", "%{file}", ":lang=zh"],
                                 capture_output=True, text=True, timeout=3).stdout.strip()
            if out and os.path.isfile(out):
                return out
        except Exception:
            pass

    return None


def server_executable_candidates():
    """按可能性从高到低列出服务端可执行文件的路径。"""
    names = [SERVER_EXE_NAME]
    names += ["gomoku_server", "gomoku_server.exe"] if IS_WINDOWS else ["gomoku_server.exe"]

    dirs = [
        os.path.join(CPP_DIR, "build", "bin"),        # CMakeLists 指定的输出目录
        os.path.join(CPP_DIR, "build"),               # 单配置生成器的默认位置
        os.path.join(CPP_DIR, "build", "Release"),    # MSVC 多配置
        os.path.join(CPP_DIR, "build", "Debug"),
        os.path.join(CPP_DIR, "build", "RelWithDebInfo"),
        CPP_DIR,
        PROJECT_ROOT,
    ]

    for directory in dirs:
        for name in names:
            yield os.path.join(directory, name)


def find_server_executable():
    override = os.environ.get(ENV_SERVER_EXE)
    if override:
        if os.path.isfile(override):
            return override
        print(f"[警告] 环境变量 {ENV_SERVER_EXE} 指向的文件不存在: {override}", file=sys.stderr)

    for path in server_executable_candidates():
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    return None


BUILD_HINT = f"""\
找不到服务端可执行文件，先编译 C++ 部分：

    cd {CPP_DIR}
    mkdir -p build && cd build
    cmake .. && cmake --build . --config Release

产物应该在 {os.path.join(CPP_DIR, "build", "bin")}/{SERVER_EXE_NAME}

也可以直接用环境变量指定现成的服务端：
    export {ENV_SERVER_EXE}=/path/to/{SERVER_EXE_NAME}
"""
