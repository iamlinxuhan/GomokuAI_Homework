"""图形客户端入口。

    python python/main.py [--host 127.0.0.1] [--port 8888] [--no-autostart] [--verbose]

先连已经在跑的服务端，连不上就自己拉起 cpp/build/bin/gomoku_server，
退出时顺手把它关掉。
"""

import argparse
import os
import sys

# 这样 `python python/main.py` 和 `python -m main` 都能跑
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import config  # noqa: E402

MISSING_DEP = "缺依赖，先跑一下：pip install -r python/requirements.txt"


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description="五子棋（Python 界面 + C++ 计算服务端）")
    parser.add_argument("--host", default=config.HOST, help="服务端地址，默认 %(default)s")
    parser.add_argument("--port", type=int, default=config.PORT,
                        help="服务端端口，默认 %(default)s")
    parser.add_argument("--no-autostart", action="store_true", help="连不上时不自动启动服务端")
    parser.add_argument("--verbose", action="store_true", help="把服务端日志打出来，便于排查")
    return parser.parse_args(argv)


def cleanup(client, server):
    client.close()
    if server is not None:
        server.stop()


def main(argv=None):
    args = parse_args(argv)

    config.HOST = args.host
    config.PORT = args.port
    if args.no_autostart:
        config.AUTO_START_SERVER = False

    try:
        import network
        import ui
    except ImportError as exc:
        print(f"缺少依赖：{exc}\n{MISSING_DEP}", file=sys.stderr)
        return 1

    try:
        client, server = network.connect_or_start(config.HOST, config.PORT, verbose=args.verbose)
    except ConnectionError as exc:
        print(f"无法建立连接：\n{exc}", file=sys.stderr)
        return 1

    try:
        app = ui.GomokuApp(client, server)
    except ImportError as exc:
        print(f"缺少依赖：{exc}\n{MISSING_DEP}", file=sys.stderr)
        cleanup(client, server)
        return 1
    except Exception as exc:
        print(f"界面初始化失败：{exc}", file=sys.stderr)
        cleanup(client, server)
        return 1

    print("[信息] Esc 返回主菜单，关闭窗口或点「离开」退出。")
    try:
        app.run()
    except KeyboardInterrupt:
        print("\n[信息] 收到中断信号，退出中……")
        app.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
