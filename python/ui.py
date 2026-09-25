"""pygame 界面。

两个状态：menu（选棋子、选难度）和 game（棋盘 + 右侧面板，结束时叠结算浮层）。
坐标约定跟服务端一致：x 是列、y 是行。
"""

import pygame

import config
import network

# gfxdraw 能画出抗锯齿的圆，缺失时就退化成普通绘制
try:
    from pygame import gfxdraw

    HAS_GFXDRAW = True
except Exception:
    HAS_GFXDRAW = False


class Button:
    def __init__(self, rect, label, action, *, primary=False, font_size=22):
        self.rect = pygame.Rect(rect)
        self.label = label
        self.action = action
        self.primary = primary
        self.font_size = font_size
        self.selected = False
        self.enabled = True
        self.hovered = False

    def hit(self, pos):
        return self.enabled and self.rect.collidepoint(pos)

    def update_hover(self, mouse_pos):
        self.hovered = self.enabled and self.rect.collidepoint(mouse_pos)

    def draw(self, app):
        if not self.enabled:
            bg, fg = config.COLOR_BUTTON_DISABLED, (110, 114, 124)
        elif self.selected:
            bg, fg = config.COLOR_BUTTON_SELECTED, (255, 255, 255)
        elif self.primary:
            bg = config.COLOR_ACCENT_HOVER if self.hovered else config.COLOR_ACCENT
            fg = (16, 20, 28)
        else:
            bg = config.COLOR_BUTTON_HOVER if self.hovered else config.COLOR_BUTTON
            fg = config.COLOR_TEXT

        pygame.draw.rect(app.screen, bg, self.rect, border_radius=8)
        if self.selected:
            pygame.draw.rect(app.screen, (150, 200, 255), self.rect, width=2, border_radius=8)

        text = app.render_text(self.label, self.font_size, fg)
        app.screen.blit(text, text.get_rect(center=self.rect.center))


class GomokuApp:
    def __init__(self, client, server=None):
        self.client = client
        self.server = server          # 由本进程启动的服务端，复用别人启动的则为 None

        self.running = True
        self.state = "menu"           # menu / game
        self.fatal_message = None

        # 对局数据，全部来自服务端的 state 消息
        self.board = [[0] * config.BOARD_SIZE for _ in range(config.BOARD_SIZE)]
        self.current_player = 1
        self.game_over = False
        self.winner = 0
        self.undo_left = config.UNDO_LIMIT
        self.thinking = False
        self.message = ""
        self.last_move = None
        self.human_player = 1
        self.ai_player = 2
        self.difficulty = 2

        self.hover_cell = None
        self.transient_error = None
        self.transient_until = 0.0
        self.thinking_started = 0.0

        self.selected_player = 1
        self.selected_difficulty = 2

        self._init_pygame()
        self._build_buttons()

    def _init_pygame(self):
        pygame.init()
        pygame.display.set_caption("五子棋 · Gomoku")
        self.screen = pygame.Surface((config.WINDOW_W, config.WINDOW_H))
        self.window = pygame.display.set_mode((config.WINDOW_W, config.WINDOW_H))
        self.clock = pygame.time.Clock()

        self.font_path = config.find_cjk_font()
        if self.font_path is None:
            print("[警告] 没找到中文字体，界面上的汉字可能显示成方块。")
            print("       装一个 fonts-noto-cjk（Debian/Ubuntu）再试。")
        self._font_cache = {}

        self.time = 0.0

    def render_text(self, text, size, color):
        font = self._font_cache.get(size)
        if font is None:
            font = pygame.font.Font(self.font_path, size) if self.font_path \
                else pygame.font.Font(None, size)
            self._font_cache[size] = font
        return font.render(text, True, color)

    def _build_buttons(self):
        cx = config.WINDOW_W // 2

        side_w, side_h, side_gap = 170, 58, 30
        side_total = side_w * 2 + side_gap
        side_x = cx - side_total // 2
        self.btn_black = Button((side_x, 292, side_w, side_h), "执黑先手",
                                lambda: self._select_player(1), font_size=23)
        self.btn_white = Button((side_x + side_w + side_gap, 292, side_w, side_h), "执白后手",
                                lambda: self._select_player(2), font_size=23)
        self.menu_buttons = [self.btn_black, self.btn_white]

        dif_w, dif_h, dif_gap = 150, 58, 16
        levels = config.MAX_DIFFICULTY - config.MIN_DIFFICULTY + 1
        dif_total = dif_w * levels + dif_gap * (levels - 1)
        dif_x = cx - dif_total // 2
        self.difficulty_buttons = []
        for level in range(config.MIN_DIFFICULTY, config.MAX_DIFFICULTY + 1):
            rect = (dif_x + (level - config.MIN_DIFFICULTY) * (dif_w + dif_gap), 432, dif_w, dif_h)
            btn = Button(rect, config.DIFFICULTY_LABELS[level],
                         lambda lv=level: self._select_difficulty(lv), font_size=20)
            self.difficulty_buttons.append(btn)
            self.menu_buttons.append(btn)

        self.btn_start = Button((cx - 130, 548, 260, 66), "开 始 游 戏",
                                self.start_game, primary=True, font_size=26)
        self.menu_buttons.append(self.btn_start)

        px = config.PANEL_X + 24
        pw = config.PANEL_W - 48
        self.btn_undo = Button((px, 246, pw, 52), "悔 棋", self.request_undo, font_size=22)
        self.btn_menu = Button((px, 310, pw, 52), "返回主菜单", self.back_to_menu, font_size=22)
        self.panel_buttons = [self.btn_undo, self.btn_menu]

        ov_w, ov_h, ov_gap = 170, 58, 28
        ov_total = ov_w * 2 + ov_gap
        ov_x = config.BOARD_AREA_W // 2 - ov_total // 2
        self.btn_again = Button((ov_x, 430, ov_w, ov_h), "再来一局",
                                self.start_game, primary=True, font_size=23)
        self.btn_quit = Button((ov_x + ov_w + ov_gap, 430, ov_w, ov_h), "离 开",
                               self.quit, font_size=23)
        self.over_buttons = [self.btn_again, self.btn_quit]

    def run(self):
        while self.running:
            self.time += self.clock.tick(config.FPS) / 1000.0
            self._handle_events()
            self._poll_network()
            self._update()
            self._draw()
            pygame.display.flip()
        self.shutdown()

    def shutdown(self):
        self.client.close()
        if self.server is not None:
            self.server.stop()
        pygame.quit()

    def quit(self):
        self.running = False

    def _handle_events(self):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.running = False
                return

            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    if self.state == "game":
                        self.back_to_menu()
                    else:
                        self.running = False
                elif event.key == pygame.K_u and self.state == "game":
                    self.request_undo()
                continue

            if self.fatal_message is not None:
                if event.type == pygame.MOUSEBUTTONDOWN:
                    self.running = False
                continue

            if event.type == pygame.MOUSEMOTION:
                if self.state == "game" and not self.game_over:
                    self.hover_cell = self._screen_to_board(event.pos)
                else:
                    self.hover_cell = None
                for btn in self._active_buttons():
                    btn.update_hover(event.pos)
                continue

            if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                self._on_click(event.pos)

    def _active_buttons(self):
        if self.state == "menu":
            return self.menu_buttons
        # 结算浮层只盖住棋盘，右侧面板仍然能用（比如输棋后想悔棋）。
        # 但浮层按钮必须等对局结束才生效：它画在棋盘正中，对局中虽然看不见，
        # 判定却是开着的，点到就是重开或者直接退出。
        if self.game_over:
            return self.panel_buttons + self.over_buttons
        return self.panel_buttons

    def _on_click(self, pos):
        for btn in self._active_buttons():
            if btn.hit(pos):
                btn.action()
                return

        if self.state != "game" or self.game_over:
            return
        if self.thinking or self.current_player != self.human_player:
            return

        cell = self._screen_to_board(pos)
        if cell is None:
            return
        x, y = cell
        if self.board[y][x] != 0:
            return

        try:
            self.client.send_move(x, y)
        except ConnectionError as exc:
            self._fatal(str(exc))
            return

        # 先本地落子，界面立刻响应，不等一个网络来回。
        # 服务端回的 state 会把棋盘覆盖掉；万一这手被判非法，
        # _show_error 会主动再拉一次 state 同步回来。
        self.board[y][x] = self.human_player
        self.last_move = (x, y)
        self.thinking = True
        self.thinking_started = self.time
        self.message = "AI正在思考中..."

    def _board_to_screen(self, x, y):
        return (config.BOARD_ORIGIN[0] + x * config.CELL,
                config.BOARD_ORIGIN[1] + y * config.CELL)

    def _screen_to_board(self, pos):
        """屏幕像素 -> (列, 行)。离交叉点超过半格或越界则返回 None。"""
        mx, my = pos
        fx = (mx - config.BOARD_ORIGIN[0]) / config.CELL
        fy = (my - config.BOARD_ORIGIN[1]) / config.CELL
        x, y = int(round(fx)), int(round(fy))
        if not (0 <= x < config.BOARD_SIZE and 0 <= y < config.BOARD_SIZE):
            return None
        if abs(fx - x) > 0.5 or abs(fy - y) > 0.5:
            return None
        return x, y

    def _poll_network(self):
        try:
            messages = self.client.poll()
        except Exception as exc:
            self._fatal(f"读取网络消息出错：{exc}")
            return

        for msg in messages:
            kind = msg.get("type")
            if kind == "state":
                self._apply_state(msg)
            elif kind == "error":
                self._show_error(msg.get("message", "服务端返回了未知错误"))
            elif kind in (network.MSG_DISCONNECTED, network.MSG_BAD_MESSAGE):
                self._fatal(msg.get("message", "与服务端的连接已断开"))

    def _apply_state(self, msg):
        try:
            board = msg["board"]
            if len(board) != config.BOARD_SIZE:
                raise ValueError("棋盘行数不对")
            for row in board:
                if len(row) != config.BOARD_SIZE:
                    raise ValueError("棋盘列数不对")
        except (KeyError, TypeError, ValueError) as exc:
            self._fatal(f"服务端返回的棋盘数据不合法：{exc}")
            return

        self.board = board
        self.current_player = msg.get("current_player", self.current_player)
        self.game_over = bool(msg.get("game_over", False))
        self.winner = msg.get("winner", 0)
        self.undo_left = msg.get("undo_left", self.undo_left)
        self.message = msg.get("message", "")
        self.thinking = bool(msg.get("thinking", False))
        self.human_player = msg.get("human_player", self.human_player)
        self.ai_player = msg.get("ai_player", self.ai_player)
        self.difficulty = msg.get("difficulty", self.difficulty)

        last = msg.get("last_move")
        if isinstance(last, dict) and "x" in last and "y" in last:
            self.last_move = (last["x"], last["y"])
        else:
            self.last_move = None

        if self.thinking:
            self.thinking_started = self.time

    def _show_error(self, text):
        """弹一条几秒后消失的提示，同时找服务端要一次 state ——
        本地可能已经乐观地画了棋子，得拿权威棋盘覆盖回来。"""
        self.transient_error = text
        self.transient_until = self.time + 4.0
        self.thinking = False
        try:
            self.client.send({"type": "state"})
        except ConnectionError as exc:
            self._fatal(str(exc))

    def _fatal(self, text):
        if self.fatal_message is None:
            self.fatal_message = text

    def _update(self):
        if self.transient_error and self.time > self.transient_until:
            self.transient_error = None

        for level, btn in enumerate(self.difficulty_buttons, start=config.MIN_DIFFICULTY):
            btn.selected = (level == self.selected_difficulty)
        self.btn_black.selected = (self.selected_player == 1)
        self.btn_white.selected = (self.selected_player == 2)

        # 棋局结束后也允许悔棋，把输掉的那手收回来接着下
        human_on_board = any(cell == self.human_player for row in self.board for cell in row)
        self.btn_undo.enabled = self.undo_left > 0 and not self.thinking and human_on_board
        self.btn_menu.enabled = not self.thinking
        self.btn_start.enabled = True

    def _draw(self):
        self.screen.fill(config.COLOR_WINDOW_BG)
        if self.state == "menu":
            self._draw_menu()
        else:
            self._draw_board()
            self._draw_panel()
            if self.game_over:
                self._draw_overlay()

        if self.transient_error:
            self._draw_toast()
        if self.fatal_message is not None:
            self._draw_fatal()

        self.window.blit(self.screen, (0, 0))

    def _draw_menu(self):
        cx = config.WINDOW_W // 2

        title = self.render_text("五 子 棋", 64, config.COLOR_TEXT)
        self.screen.blit(title, title.get_rect(center=(cx, 132)))

        subtitle = self.render_text("Gomoku · 人机对战", 22, config.COLOR_TEXT_DIM)
        self.screen.blit(subtitle, subtitle.get_rect(center=(cx, 192)))

        pygame.draw.line(self.screen, config.COLOR_PANEL_LINE,
                         (cx - 220, 228), (cx + 220, 228), 1)

        for text, y in (("选择你的棋子", 262), ("选择 AI 难度", 402)):
            label = self.render_text(text, 24, config.COLOR_TEXT)
            self.screen.blit(label, label.get_rect(center=(cx, y)))

        for btn in self.menu_buttons:
            btn.draw(self)

        hint = self.render_text(
            f"黑棋先行 · 每局可悔棋 {config.UNDO_LIMIT} 次 · 难度越高 AI 思考越久",
            18, config.COLOR_TEXT_DIM)
        self.screen.blit(hint, hint.get_rect(center=(cx, 664)))

    def _draw_board(self):
        ox, oy = config.BOARD_ORIGIN
        pad = config.BOARD_PAD
        left, top = ox - pad, oy - pad
        right = ox + config.BOARD_PIXELS + pad
        bottom = oy + config.BOARD_PIXELS + pad

        pygame.draw.rect(self.screen, config.COLOR_BOARD_BG,
                         (left, top, right - left, bottom - top), border_radius=6)
        pygame.draw.rect(self.screen, config.COLOR_BOARD_EDGE,
                         (left, top, right - left, bottom - top), width=3, border_radius=6)

        for i in range(config.BOARD_SIZE):
            px, py = self._board_to_screen(i, 0)
            _, py_end = self._board_to_screen(i, config.BOARD_SIZE - 1)
            pygame.draw.line(self.screen, config.COLOR_GRID, (px, py), (px, py_end), 1)

            px0, py0 = self._board_to_screen(0, i)
            px_end, _ = self._board_to_screen(config.BOARD_SIZE - 1, i)
            pygame.draw.line(self.screen, config.COLOR_GRID, (px0, py0), (px_end, py0), 1)

        for sx, sy in config.STAR_POINTS:
            if 0 <= sx < config.BOARD_SIZE and 0 <= sy < config.BOARD_SIZE:
                pygame.draw.circle(self.screen, config.COLOR_STAR,
                                   self._board_to_screen(sx, sy), 3)

        # 坐标 1~18。行号得放在木色底板外面，否则两位数的头一个字符会压到深色背景上
        label_x = ox - pad - 18
        for i in range(config.BOARD_SIZE):
            text = self.render_text(str(i + 1), 15, (96, 70, 42))
            px, _ = self._board_to_screen(i, 0)
            self.screen.blit(text, text.get_rect(center=(px, oy - 22)))
            _, py = self._board_to_screen(0, i)
            self.screen.blit(text, text.get_rect(center=(label_x, py)))

        if (self.hover_cell and not self.thinking and not self.game_over
                and self.current_player == self.human_player):
            hx, hy = self.hover_cell
            if self.board[hy][hx] == 0:
                self._draw_ghost(self._board_to_screen(hx, hy), self.human_player)

        for y in range(config.BOARD_SIZE):
            for x in range(config.BOARD_SIZE):
                if self.board[y][x]:
                    self._draw_stone(self._board_to_screen(x, y), self.board[y][x])

        if self.last_move:
            lx, ly = self.last_move
            if 0 <= lx < config.BOARD_SIZE and 0 <= ly < config.BOARD_SIZE and self.board[ly][lx]:
                pygame.draw.circle(self.screen, config.COLOR_LAST_MOVE,
                                   self._board_to_screen(lx, ly),
                                   config.STONE_RADIUS + 4, 2)

    def _draw_stone(self, center, player, radius=None):
        radius = radius or config.STONE_RADIUS
        if player == 1:
            body, edge, gloss = (config.COLOR_STONE_BLACK, config.COLOR_STONE_BLACK_EDGE,
                                 (86, 86, 96))
        else:
            body, edge, gloss = (config.COLOR_STONE_WHITE, config.COLOR_STONE_WHITE_EDGE,
                                 (255, 255, 255))

        # 阴影画在不透明底板上，所以用深一点的木色而不是半透明黑
        pygame.draw.circle(self.screen, config.COLOR_BOARD_SHADOW,
                           (center[0] + 2, center[1] + 2), radius)

        if HAS_GFXDRAW:
            gfxdraw.filled_circle(self.screen, center[0], center[1], radius, body)
            gfxdraw.aacircle(self.screen, center[0], center[1], radius, edge)
            gfxdraw.filled_circle(self.screen, center[0] - radius // 3,
                                  center[1] - radius // 3, max(2, radius // 3), gloss)
        else:
            pygame.draw.circle(self.screen, body, center, radius)
            pygame.draw.circle(self.screen, edge, center, radius, 1)
            pygame.draw.circle(self.screen, gloss,
                               (center[0] - radius // 3, center[1] - radius // 3),
                               max(2, radius // 3))

    def _draw_ghost(self, center, player):
        radius = config.STONE_RADIUS
        size = radius * 2 + 4
        surf = pygame.Surface((size, size), pygame.SRCALPHA)
        color = (24, 24, 28, 90) if player == 1 else (255, 255, 255, 130)
        pygame.draw.circle(surf, color, (size // 2, size // 2), radius)
        self.screen.blit(surf, (center[0] - size // 2, center[1] - size // 2))

    def _draw_panel(self):
        pygame.draw.rect(self.screen, config.COLOR_PANEL_BG,
                         (config.PANEL_X, 0, config.PANEL_W, config.WINDOW_H))
        pygame.draw.line(self.screen, config.COLOR_PANEL_LINE,
                         (config.PANEL_X, 0), (config.PANEL_X, config.WINDOW_H), 1)

        px = config.PANEL_X + 24
        width = config.PANEL_W - 48

        title = self.render_text("对局信息", 26, config.COLOR_TEXT)
        self.screen.blit(title, (px, 34))
        pygame.draw.line(self.screen, config.COLOR_PANEL_LINE, (px, 74), (px + width, 74), 1)

        if self.thinking:
            status = f"AI正在思考中{'.' * (int(self.time * 3) % 4)}"
            color = config.COLOR_WARNING
        elif self.game_over:
            status, color = self.message or "本局结束", config.COLOR_ACCENT
        else:
            status = self.message or "轮到你落子"
            color = (config.COLOR_SUCCESS if self.current_player == self.human_player
                     else config.COLOR_TEXT_DIM)
        self.screen.blit(self.render_text(status, 22, color), (px, 96))

        if self.game_over:
            turn_text = "对局已结束"
        elif self.thinking or self.current_player != self.human_player:
            turn_text = "轮到：AI"
        else:
            turn_text = "轮到：你"
        self.screen.blit(self.render_text(turn_text, 19, config.COLOR_TEXT_DIM), (px, 132))

        undo_color = config.COLOR_TEXT if self.undo_left > 0 else config.COLOR_DANGER
        self.screen.blit(self.render_text(
            f"剩余悔棋：{self.undo_left} / {config.UNDO_LIMIT} 次", 19, undo_color), (px, 160))

        side_text = "你执黑棋" if self.human_player == 1 else "你执白棋"
        self.screen.blit(self.render_text(
            f"{side_text} · AI 难度 {self.difficulty} 级", 18, config.COLOR_TEXT_DIM), (px, 190))

        for btn in self.panel_buttons:
            btn.draw(self)

        tip_y = 400
        pygame.draw.line(self.screen, config.COLOR_PANEL_LINE,
                         (px, tip_y - 20), (px + width, tip_y - 20), 1)
        self.screen.blit(self.render_text("操作提示", 20, config.COLOR_TEXT), (px, tip_y))

        tips = ["鼠标左键点击交叉点落子",
                "点击「悔棋」回退你和 AI 各一步",
                "快捷键：U 悔棋 / Esc 返回菜单"]
        for i, tip in enumerate(tips):
            self.screen.blit(self.render_text("· " + tip, 16, config.COLOR_TEXT_DIM),
                             (px, tip_y + 36 + i * 28))

        stones = sum(1 for row in self.board for cell in row if cell)
        self.screen.blit(self.render_text(f"已落子：{stones} 手", 16, config.COLOR_TEXT_DIM),
                         (px, config.WINDOW_H - 44))

    def _draw_overlay(self):
        overlay = pygame.Surface((config.BOARD_AREA_W, config.WINDOW_H), pygame.SRCALPHA)
        overlay.fill((18, 20, 26, 200))
        self.screen.blit(overlay, (0, 0))

        cx = config.BOARD_AREA_W // 2
        if self.winner == 0:
            result, color = "平 局", config.COLOR_WARNING
            detail = "棋盘已经下满，势均力敌"
        elif self.winner == self.human_player:
            result, color = "你 赢 了 ！", config.COLOR_SUCCESS
            detail = f"恭喜战胜难度 {self.difficulty} 级的 AI"
        else:
            result, color = "你 输 了", config.COLOR_DANGER
            detail = "再接再厉，试试悔棋或者降低难度"

        text = self.render_text(result, 58, color)
        self.screen.blit(text, text.get_rect(center=(cx, 262)))

        text = self.render_text(detail, 22, config.COLOR_TEXT_DIM)
        self.screen.blit(text, text.get_rect(center=(cx, 330)))

        for btn in self.over_buttons:
            btn.draw(self)

    def _draw_toast(self):
        text = self.render_text(self.transient_error, 20, (255, 255, 255))
        pad = 18
        w = text.get_width() + pad * 2
        h = text.get_height() + pad
        x = (config.BOARD_AREA_W - w) // 2
        y = config.WINDOW_H - 92

        box = pygame.Surface((w, h), pygame.SRCALPHA)
        box.fill((190, 60, 60, 225))
        pygame.draw.rect(box, (240, 120, 120), box.get_rect(), width=1, border_radius=8)
        self.screen.blit(box, (x, y))
        self.screen.blit(text, text.get_rect(center=(x + w // 2, y + h // 2)))

    def _draw_fatal(self):
        overlay = pygame.Surface((config.WINDOW_W, config.WINDOW_H), pygame.SRCALPHA)
        overlay.fill((10, 10, 14, 225))
        self.screen.blit(overlay, (0, 0))

        cx = config.WINDOW_W // 2
        title = self.render_text("连接已断开", 44, config.COLOR_DANGER)
        self.screen.blit(title, title.get_rect(center=(cx, 260)))

        for i, line in enumerate(str(self.fatal_message).split("\n")):
            text = self.render_text(line, 20, config.COLOR_TEXT)
            self.screen.blit(text, text.get_rect(center=(cx, 330 + i * 30)))

        hint = self.render_text("点击任意位置退出", 20, config.COLOR_TEXT_DIM)
        self.screen.blit(hint, hint.get_rect(center=(cx, 470)))

    def _select_player(self, player):
        self.selected_player = player

    def _select_difficulty(self, level):
        self.selected_difficulty = level

    def start_game(self):
        self.human_player = self.selected_player
        self.ai_player = 3 - self.selected_player
        self.difficulty = self.selected_difficulty

        # 先本地清空，等服务端的 state 回来再覆盖
        self.board = [[0] * config.BOARD_SIZE for _ in range(config.BOARD_SIZE)]
        self.game_over = False
        self.winner = 0
        self.last_move = None
        self.undo_left = config.UNDO_LIMIT
        self.hover_cell = None
        self.transient_error = None

        # 执白的话开局由 AI 先走，先显示思考提示
        self.thinking = (self.human_player == 2)
        self.thinking_started = self.time

        try:
            self.client.send_new_game(self.selected_player, self.selected_difficulty)
        except ConnectionError as exc:
            self._fatal(str(exc))
            return

        self.state = "game"

    def request_undo(self):
        if not self.btn_undo.enabled:
            return
        try:
            self.client.send_undo()
        except ConnectionError as exc:
            self._fatal(str(exc))

    def back_to_menu(self):
        if self.thinking:
            return
        self.state = "menu"
        self.game_over = False
        self.hover_cell = None
        self.transient_error = None
