# PyInstaller 打包配置。用环境变量控制打哪个平台、哪种形态：
#
#   GOMOKU_SERVER_BIN  编译好的 C++ 服务端二进制路径（必需）
#   GOMOKU_FONT        要捆绑的中文字体路径（可选）
#   GOMOKU_ONEFILE     "1" 打单文件，否则打目录（绿色便携版）
#   GOMOKU_ICON        .ico / .icns 图标路径（可选）
#
# 命令：pyinstaller --clean --noconfirm gomoku.spec

import os
import sys

onefile = os.environ.get("GOMOKU_ONEFILE") == "1"

# macOS 的目录形态要裹成 .app，不然没有 Dock 图标、双击也起不来。
make_app = sys.platform == "darwin" and not onefile

server_bin = os.environ.get("GOMOKU_SERVER_BIN")
if not server_bin or not os.path.isfile(server_bin):
    raise SystemExit(f"GOMOKU_SERVER_BIN 没设对，找不到文件: {server_bin!r}")

# 服务端必须走 binaries 而不是 datas：datas 解压出来不带可执行位，
# Linux / macOS 上 config.py 的 os.access(path, X_OK) 会漏判。
binaries = [(server_bin, ".")]

datas = []
font = os.environ.get("GOMOKU_FONT")
if font and os.path.isfile(font):
    datas.append((font, "fonts"))

icon = os.environ.get("GOMOKU_ICON")
if icon and not os.path.isfile(icon):
    icon = None

a = Analysis(
    ["main.py"],
    pathex=[os.path.dirname(os.path.abspath(SPEC))],
    binaries=binaries,
    datas=datas,
    hiddenimports=["pygame.gfxdraw"],
    excludes=["tkinter", "unittest", "pydoc_data", "test"],
    noarchive=False,
)

pyz = PYZ(a.pure)

if onefile:
    exe = EXE(
        pyz,
        a.scripts,
        a.binaries,
        a.datas,
        [],
        name="Gomoku",
        debug=False,
        strip=False,
        upx=False,
        runtime_tmpdir=None,
        console=False,
        icon=icon,
    )
else:
    exe = EXE(
        pyz,
        a.scripts,
        [],
        exclude_binaries=True,
        name="Gomoku",
        debug=False,
        strip=False,
        upx=False,
        console=False,
        icon=icon,
    )
    coll = COLLECT(
        exe,
        a.binaries,
        a.datas,
        strip=False,
        upx=False,
        name="Gomoku",
    )
    if make_app:
        app = BUNDLE(
            coll,
            name="Gomoku.app",
            icon=icon,
            bundle_identifier="com.iamlinxuhan.gomoku",
            info_plist={
                "CFBundleName": "五子棋",
                "CFBundleDisplayName": "五子棋",
                "NSHighResolutionCapable": True,
                "LSMinimumSystemVersion": "11.0",
                # pygame 用 SDL，属于「不受信任的输入监控」，不加这句在
                # 较新的 macOS 上第一次启动会被输入监控提示挡住。
                "NSInputMonitoringUsageDescription": "用于接收键盘和鼠标事件。",
            },
        )
