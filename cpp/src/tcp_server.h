#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>

namespace gomoku {

// 每个连接一个线程，各自持有独立的棋局。
// socket 句柄统一用 std::intptr_t 存，这样头文件里不用出现系统头。
class TcpServer {
public:
    TcpServer(std::string host, int port);
    ~TcpServer();

    bool bindAndListen(std::string& error);
    void serve();  // 阻塞，直到 stop() / requestStop() 被调用

    void requestStop() { stopping_ = true; }  // 只置标志，信号处理函数里可以安全调用
    void stop();                              // 关连接并等工作线程退出

    int port() const { return port_; }

private:
    void serveClient(std::intptr_t sock);

    std::string host_;
    int port_;
    std::intptr_t listener_ = -1;
    std::atomic<bool> stopping_{false};
    int workers_ = 0;
    std::set<std::intptr_t> clients_;
    std::mutex mtx_;
    std::condition_variable idle_;
};

}  // namespace gomoku
