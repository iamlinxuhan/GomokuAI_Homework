// 五子棋计算服务端：维护棋局 + 跑 AI，通过 TCP 用 JSON 行协议和 pygame 客户端通信。
// 界面的事全在 Python 那边，这里不碰任何平台相关的窗口 / 控制台 API。

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "game.h"
#include "tcp_server.h"

namespace {

gomoku::TcpServer* g_server = nullptr;

// 信号处理函数里只做一个原子写，别在这儿加锁或者做 IO
void onSignal(int) {
    if (g_server) g_server->requestStop();
}

void usage(const char* exe) {
    std::printf(
        "用法: %s [选项]\n"
        "  --host <地址>   监听地址，默认 127.0.0.1\n"
        "  --port <端口>   监听端口，默认 8888\n"
        "  --help          显示帮助\n",
        exe);
}

bool parseArgs(int argc, char** argv, std::string& host, int& port) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            std::exit(0);
        }
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
            continue;
        }
        if (arg == "--port" && i + 1 < argc) {
            const int value = std::atoi(argv[++i]);
            if (value < 0 || value > 65535) {
                std::fprintf(stderr, "端口要在 0~65535 之间\n");
                return false;
            }
            port = value;
            continue;
        }
        std::fprintf(stderr, "无法识别的参数: %s\n", arg.c_str());
        usage(argv[0]);
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    int port = 8888;
    if (!parseArgs(argc, argv, host, port)) return 2;

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);  // 对端断线时别让 SIGPIPE 直接干掉进程
#endif

    gomoku::TcpServer server(host, port);
    g_server = &server;

    std::string error;
    if (!server.bindAndListen(error)) {
        std::fprintf(stderr, "启动失败: %s\n", error.c_str());
        return 1;
    }

    std::printf("五子棋服务端已启动: %s:%d\n", host.c_str(), server.port());
    std::printf("棋盘 %d×%d，连 5 子获胜，每局可悔棋 %d 次。Ctrl+C 退出。\n", gomoku::SIZE,
                gomoku::SIZE, gomoku::UNDO_LIMIT);
    std::fflush(stdout);

    server.serve();
    server.stop();

    g_server = nullptr;
    std::printf("服务端已退出。\n");
    return 0;
}
