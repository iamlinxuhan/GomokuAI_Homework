#include "tcp_server.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "game.h"
#include "protocol.h"

namespace gomoku {
namespace {

using Sock = std::intptr_t;

#ifdef _WIN32
using Native = SOCKET;
constexpr int kSendFlags = 0;
#else
using Native = int;
#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif
#endif

Native fd(Sock s) { return static_cast<Native>(s); }

#ifdef _WIN32

// WSAStartup 一次就够，靠静态对象管生命周期
struct WinsockInit {
    WinsockInit() {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
    }
    ~WinsockInit() { WSACleanup(); }
};

std::string lastError() { return "错误码 " + std::to_string(WSAGetLastError()); }

#else

std::string lastError() { return std::strerror(errno); }

#endif

void closeSock(Sock s) {
    if (s < 0) return;
#ifdef _WIN32
    ::closesocket(fd(s));
#else
    ::close(fd(s));
#endif
}

// recv() 阻塞在别的线程里时 close() 唤不醒它，必须先 shutdown()
void wakeSock(Sock s) {
#ifdef _WIN32
    ::shutdown(fd(s), SD_BOTH);
#else
    ::shutdown(fd(s), SHUT_RDWR);
#endif
}

bool sendAll(Sock s, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        const int chunk = static_cast<int>(std::min<size_t>(data.size() - sent, 1 << 20));
        const int n = static_cast<int>(::send(fd(s), data.data() + sent, chunk, kSendFlags));
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

void tuneSocket(Sock s) {
    const int on = 1;
    ::setsockopt(fd(s), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
#ifdef SO_NOSIGPIPE
    ::setsockopt(fd(s), SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const char*>(&on), sizeof(on));
#endif
}

constexpr size_t kMaxLine = 1 << 20;  // 单行上限，防止客户端灌垃圾把内存撑爆

// 一个连接的全部状态
struct Client {
    Sock sock = -1;
    Game game;
    std::string buf;
};

bool reply(Client& c, const json& msg) { return sendAll(c.sock, encodeLine(msg)); }

// 玩家落子 / 开局之后统一从这里出状态。
// 轮到 AI 就先回一条 thinking=true，让界面立刻把刚下的一手画出来，算完再回最终结果。
bool afterMove(Client& c) {
    if (c.game.aiToMove()) {
        if (!reply(c, c.game.toJson(true, "AI正在思考中..."))) return false;
        c.game.playAi();
    }
    return reply(c, c.game.toJson(false, c.game.describe()));
}

bool dispatch(Client& c, const std::string& line) {
    json msg;
    try {
        msg = json::parse(line);
    } catch (const json::exception& e) {
        return reply(c, errorMsg(std::string("不是合法的 JSON：") + e.what()));
    }
    if (!msg.is_object()) return reply(c, errorMsg("消息必须是一个 JSON 对象"));

    const std::string type = msg.value("type", "");
    if (type.empty()) return reply(c, errorMsg("消息缺少 type 字段"));

    // 字段类型不对时 nlohmann 会抛异常，统一在这里兜住
    try {
        if (type == "new_game") {
            const int player = msg.value("player", BLACK);
            if (player != BLACK && player != WHITE) {
                return reply(c, errorMsg("player 只能是 1（黑）或 2（白）"));
            }
            c.game.reset(player, msg.value("difficulty", 2));
            return afterMove(c);
        }
        if (type == "move") {
            const std::string err = c.game.playHuman(msg.value("x", -1), msg.value("y", -1));
            if (!err.empty()) return reply(c, errorMsg(err));
            return afterMove(c);
        }
        if (type == "undo") {
            const std::string err = c.game.undo();
            if (!err.empty()) return reply(c, errorMsg(err));
            return reply(c, c.game.toJson(false, c.game.describe()));
        }
        if (type == "state") {
            return reply(c, c.game.toJson(false, c.game.describe()));
        }
        return reply(c, errorMsg("未知的消息类型：" + type));
    } catch (const json::exception& e) {
        return reply(c, errorMsg(std::string("消息字段不合法：") + e.what()));
    }
}

}  // namespace

TcpServer::TcpServer(std::string host, int port) : host_(std::move(host)), port_(port) {}

TcpServer::~TcpServer() { stop(); }

bool TcpServer::bindAndListen(std::string& error) {
#ifdef _WIN32
    static WinsockInit winsock;
    (void)winsock;
#endif

    const Sock s = static_cast<Sock>(::socket(AF_INET, SOCK_STREAM, 0));
    if (s < 0) {
        error = "创建 socket 失败：" + lastError();
        return false;
    }

    const int on = 1;
    ::setsockopt(fd(s), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof(on));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(port_));
    if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
        error = "无法解析地址：" + host_;
        closeSock(s);
        return false;
    }

    if (::bind(fd(s), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        error = "绑定 " + host_ + ":" + std::to_string(port_) + " 失败：" + lastError();
        closeSock(s);
        return false;
    }

    if (::listen(fd(s), 8) != 0) {
        error = "监听失败：" + lastError();
        closeSock(s);
        return false;
    }

    // 端口传 0 时由系统分配，这里把实际端口读回来
    sockaddr_in bound{};
    socklen_t len = sizeof(bound);
    if (::getsockname(fd(s), reinterpret_cast<sockaddr*>(&bound), &len) == 0) {
        port_ = ntohs(bound.sin_port);
    }

    listener_ = s;
    return true;
}

void TcpServer::serve() {
    while (!stopping_) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(fd(listener_), &readable);

        // 半秒醒一次，这样 stop() 不用等太久
        timeval wait{0, 500000};
        const int ready =
            ::select(static_cast<int>(fd(listener_)) + 1, &readable, nullptr, nullptr, &wait);
        if (ready < 0) {
            if (stopping_) break;
            continue;
        }
        if (ready == 0) continue;

        const Sock client = static_cast<Sock>(::accept(fd(listener_), nullptr, nullptr));
        if (client < 0) continue;
        tuneSocket(client);

        {
            std::lock_guard<std::mutex> lock(mtx_);
            clients_.insert(client);
            ++workers_;
        }
        const Sock sock = client;
        std::thread([this, sock] { serveClient(sock); }).detach();
    }
}

void TcpServer::serveClient(Sock sock) {
    Client c;
    c.sock = sock;

    char buf[4096];
    bool alive = true;
    while (alive && !stopping_) {
        const int n = static_cast<int>(::recv(fd(sock), buf, sizeof(buf), 0));
        if (n <= 0) break;

        c.buf.append(buf, static_cast<size_t>(n));
        if (c.buf.size() > kMaxLine) break;

        size_t pos;
        while (alive && (pos = c.buf.find('\n')) != std::string::npos) {
            std::string line = c.buf.substr(0, pos);
            c.buf.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) alive = dispatch(c, line);
        }
    }

    // 先从名单里摘掉再关，免得 stop() 拿到已经复用的句柄
    {
        std::lock_guard<std::mutex> lock(mtx_);
        clients_.erase(sock);
        --workers_;
    }
    closeSock(sock);
    idle_.notify_all();
}

void TcpServer::stop() {
    stopping_ = true;

    if (listener_ >= 0) {
        closeSock(listener_);
        listener_ = -1;
    }

    std::unique_lock<std::mutex> lock(mtx_);
    for (Sock s : clients_) wakeSock(s);
    idle_.wait(lock, [this] { return workers_ == 0; });
}

}  // namespace gomoku
