"""网络层：拉起 C++ 服务端、连上去、收发 JSON 行协议。

收数据放在独立线程里，解析好的消息丢进 queue，渲染循环用 poll() 非阻塞地取，
所以 AI 思考时界面照样刷新。断线一类的异常也包装成消息，不往渲染循环里抛。
"""

import json
import os
import queue
import socket
import subprocess
import sys
import threading
import time

import config

# 本地合成的消息类型，服务端不会发这两个
MSG_DISCONNECTED = "_disconnected"
MSG_BAD_MESSAGE = "_bad_message"


class ServerProcess:
    """按需拉起编译好的服务端，退出时负责回收。"""

    def __init__(self, exe_path):
        self.exe_path = exe_path
        self._proc = None

    @property
    def running(self):
        return self._proc is not None and self._proc.poll() is None

    def start(self, host, port, verbose=False):
        if self.running:
            return

        cmd = [self.exe_path, "--host", host, "--port", str(port)]
        if verbose:
            kwargs = {"stdout": None, "stderr": None}   # 继承终端输出，方便排查
        else:
            kwargs = {"stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL}
        if os.name == "nt":
            kwargs["creationflags"] = getattr(subprocess, "CREATE_NO_WINDOW", 0)

        try:
            self._proc = subprocess.Popen(cmd, **kwargs)
        except OSError as exc:
            raise ConnectionError(f"无法启动服务端 {self.exe_path}: {exc}") from exc

    def stop(self):
        proc = self._proc
        self._proc = None
        if proc is None or proc.poll() is not None:
            return

        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass


def port_is_open(host, port, timeout=0.2):
    """端口上有没有活着的监听者。

    TIME_WAIT 里的旧连接不算占着：服务端 bind 时带了 SO_REUSEADDR，
    这种情况它照样绑得上，所以这里用 connect 而不是 bind 来判断。
    """
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def is_our_server(host, port, timeout=0.6):
    """端口上跑的是不是我们的服务端。

    光看「端口开着」不够：占着 8888 的完全可能是别的程序，那样连上去也是白连。
    所以真发一条只读的 state 过去，回得出 JSON 状态的就认。
    """
    try:
        with socket.create_connection((host, port), timeout=timeout) as sock:
            sock.settimeout(timeout)
            sock.sendall(b'{"type":"state"}\n')

            buf = b""
            while b"\n" not in buf:
                chunk = sock.recv(4096)
                if not chunk:
                    return False
                buf += chunk
    except OSError:
        return False

    line = buf.split(b"\n", 1)[0].strip()
    if not line:
        return False
    try:
        msg = json.loads(line.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        return False
    return isinstance(msg, dict) and msg.get("type") == "state"


def wait_for_port(host, port, timeout, proc=None):
    """等端口可连接；proc 中途退出就直接报错，不用干等到超时。"""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if proc is not None and proc.poll() is not None:
            raise ConnectionError(
                f"服务端进程在 {host}:{port} 上启动失败（退出码 {proc.returncode}）")
        try:
            with socket.create_connection((host, port), timeout=0.5):
                return True
        except OSError:
            time.sleep(0.1)
    return False


class GomokuClient:
    def __init__(self, host=config.HOST, port=config.PORT):
        self.host = host
        self.port = port
        self._sock = None
        self._queue = queue.Queue()
        self._recv_thread = None
        self._stop_event = threading.Event()
        self._send_lock = threading.Lock()

    @property
    def connected(self):
        return self._sock is not None

    def connect(self, timeout=config.CONNECT_TIMEOUT):
        if self.connected:
            return

        self._sock = socket.create_connection((self.host, self.port), timeout=timeout)
        self._sock.settimeout(config.RECV_TIMEOUT)
        try:
            self._sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        except OSError:
            pass

        self._stop_event.clear()
        self._recv_thread = threading.Thread(
            target=self._receive_loop, name="gomoku-recv", daemon=True)
        self._recv_thread.start()

    def close(self):
        self._stop_event.set()

        sock, self._sock = self._sock, None
        if sock is not None:
            try:
                sock.shutdown(socket.SHUT_RDWR)   # 先把阻塞在 recv 的线程放出来
            except OSError:
                pass
            try:
                sock.close()
            except OSError:
                pass

        if self._recv_thread is not None:
            self._recv_thread.join(timeout=2.0)
            self._recv_thread = None

    def send(self, message):
        sock = self._sock
        if sock is None:
            raise ConnectionError("尚未连接到服务端")

        data = (json.dumps(message, ensure_ascii=False) + "\n").encode("utf-8")
        with self._send_lock:
            try:
                sock.sendall(data)
            except OSError as exc:
                raise ConnectionError(f"发送失败: {exc}") from exc

    def send_move(self, x, y):
        self.send({"type": "move", "x": int(x), "y": int(y)})

    def send_new_game(self, player, difficulty):
        self.send({"type": "new_game", "player": int(player), "difficulty": int(difficulty)})

    def send_undo(self):
        self.send({"type": "undo"})

    def _receive_loop(self):
        buffer = b""
        while not self._stop_event.is_set():
            sock = self._sock          # close() 随时会把它置成 None，先抓一份
            if sock is None:
                return
            try:
                chunk = sock.recv(65536)
            except OSError as exc:
                if not self._stop_event.is_set():
                    self._queue.put({"type": MSG_DISCONNECTED,
                                     "message": f"与服务端的连接中断：{exc}"})
                return

            if not chunk:
                if not self._stop_event.is_set():
                    self._queue.put({"type": MSG_DISCONNECTED,
                                     "message": "服务端关闭了连接"})
                return

            buffer += chunk
            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                line = line.strip()
                if not line:
                    continue
                try:
                    self._queue.put(json.loads(line.decode("utf-8")))
                except (ValueError, UnicodeDecodeError) as exc:
                    self._queue.put({"type": MSG_BAD_MESSAGE,
                                     "message": f"收到无法解析的数据：{exc}"})

    def poll(self):
        """取出当前已到达的全部消息，没有就返回空列表。"""
        messages = []
        while True:
            try:
                messages.append(self._queue.get_nowait())
            except queue.Empty:
                return messages


def connect_or_start(host=config.HOST, port=None, verbose=False):
    """返回 (client, server)。

    端口池里已经有服务端就直接连，返回的 server 是 None（说明这个进程不归我们管）；
    否则自己拉起可执行文件，退出时负责关掉。

    端口是一个个顺延着试的：8888 被别的程序占着就换 8889，直到找出一个能用的，
    免得端口一冲突整个程序就起不来。
    """
    ports = config.port_pool(port)

    for candidate in ports:
        if is_our_server(host, candidate):
            client = GomokuClient(host, candidate)
            client.connect()
            print(f"[信息] 已连接到正在运行的服务端 {host}:{candidate}")
            return client, None

    if not config.AUTO_START_SERVER:
        raise ConnectionError(
            f"{host} 的 {ports[0]}~{ports[-1]} 上没有正在运行的服务端，"
            "而配置里关掉了自动启动服务端。\n"
            "请先手动运行服务端，或把 config.AUTO_START_SERVER 改成 True。")

    exe = config.find_server_executable()
    if exe is None:
        raise ConnectionError("找不到服务端可执行文件。\n\n" + config.BUILD_HINT)

    last_error = None
    for candidate in ports:
        if port_is_open(host, candidate):
            continue

        server = ServerProcess(exe)
        server.start(host, candidate, verbose=verbose)
        print(f"[信息] 已启动服务端：{exe} (pid={server._proc.pid}) 端口 {candidate}")

        try:
            ready = wait_for_port(host, candidate, config.SERVER_START_TIMEOUT,
                                  proc=server._proc)
        except ConnectionError as exc:
            # 进程自己退了，基本就是 bind 失败，顺延下一个端口再试
            last_error = exc
            server.stop()
            print(f"[警告] {host}:{candidate} 起不来，换下一个端口", file=sys.stderr)
            continue

        if not ready:
            server.stop()
            raise ConnectionError(
                f"服务端在 {host}:{candidate} 上起来了，"
                f"{config.SERVER_START_TIMEOUT:.0f} 秒内却没开始监听。")

        client = GomokuClient(host, candidate)
        client.connect()
        print(f"[信息] 已连接到服务端 {host}:{candidate}")
        return client, server

    detail = (f"最后一条错误：{last_error}" if last_error
              else "这些端口上都已经有别的程序在监听。")
    raise ConnectionError(
        f"{host} 的 {ports[0]}~{ports[-1]} 这 {len(ports)} 个端口都没能用。\n"
        f"{detail}\n"
        "关掉占着端口的程序，或者改一下 python/config.py 里的 PORT。")
