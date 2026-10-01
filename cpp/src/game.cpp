#include "game.h"

#include <cstring>

#include "ai.h"

namespace gomoku {

void Game::reset(int humanSide, int difficulty) {
    std::memset(cells_, 0, sizeof(cells_));
    history_.clear();
    hasLast_ = false;
    over_ = false;
    winner_ = EMPTY;
    undoLeft_ = UNDO_LIMIT;

    human_ = (humanSide == BLACK || humanSide == WHITE) ? humanSide : BLACK;
    ai_ = opponent(human_);
    difficulty_ = difficulty < 1 ? 1 : (difficulty > 4 ? 4 : difficulty);
    turn_ = BLACK;  // 黑先
}

std::string Game::playHuman(int x, int y) {
    if (over_) return "对局已经结束了";
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) return "落子越界，坐标应在 0~17 之间";
    if (cells_[y][x] != EMPTY) return "非法落子：该位置已经有棋子了";
    if (turn_ != human_) return "还没轮到你落子";

    put(x, y, human_);
    return "";
}

bool Game::playAi() {
    if (!aiToMove()) return false;

    int a = SIZE / 2 - 1;  //ai落子的坐标，跟原版一样叫 a、b
    int b = SIZE / 2 - 1;
    if (!history_.empty()) chooseMove(cells_, ai_, difficulty_, a, b);

    // AI 理论上不会给非法点，兜底随便找个空位
    if (a < 0 || a >= SIZE || b < 0 || b >= SIZE || cells_[b][a] != EMPTY) {
        bool ok = false;
        for (int j = 0; j < SIZE && !ok; ++j) {
            for (int i = 0; i < SIZE && !ok; ++i) {
                if (cells_[j][i] == EMPTY) {
                    a = i;
                    b = j;
                    ok = true;
                }
            }
        }
        if (!ok) return false;
    }

    put(a, b, ai_);
    return true;
}

std::string Game::undo() {
    if (history_.empty()) return "还没有落子，无法悔棋";
    if (undoLeft_ <= 0) return "悔棋次数已经用完";

    // 先撤掉玩家那一手之后 AI 的应手，再撤掉玩家那一手
    while (!history_.empty() && history_.back().player != human_) takeBack();
    if (!history_.empty()) takeBack();

    --undoLeft_;
    over_ = false;
    winner_ = EMPTY;
    turn_ = history_.size() % 2 == 0 ? BLACK : WHITE;
    return "";
}

json Game::toJson(bool thinking, const std::string& message) const {
    json board = json::array();
    for (int y = 0; y < SIZE; ++y) {
        json row = json::array();
        for (int x = 0; x < SIZE; ++x) row.push_back(cells_[y][x]);
        board.push_back(row);
    }

    json msg = {
        {"type", "state"},
        {"board", board},
        {"current_player", turn_},
        {"game_over", over_},
        {"winner", winner_},
        {"undo_left", undoLeft_},
        {"message", message},
        {"thinking", thinking},
        {"human_player", human_},
        {"ai_player", ai_},
        {"difficulty", difficulty_},
        {"move_count", static_cast<int>(history_.size())},
        {"last_move", hasLast_ ? json{{"x", last_.x}, {"y", last_.y}, {"player", last_.player}}
                               : json(nullptr)},
    };
    return msg;
}

std::string Game::describe() const {
    if (over_) {
        if (winner_ == EMPTY) return "平局！棋盘已经下满";
        return winner_ == human_ ? "你赢了！" : "你输了。";
    }
    return turn_ == human_ ? "轮到你落子" : "轮到 AI 思考";
}

void Game::put(int x, int y, int player) {
    cells_[y][x] = player;
    last_ = Move{x, y, player};
    hasLast_ = true;
    history_.push_back(last_);

    if (fiveAt(x, y, player)) {
        over_ = true;
        winner_ = player;
    } else if (boardFull()) {
        over_ = true;
        winner_ = EMPTY;
    } else {
        turn_ = opponent(player);
    }
}

void Game::takeBack() {
    if (history_.empty()) return;

    const Move& m = history_.back();
    cells_[m.y][m.x] = EMPTY;
    history_.pop_back();

    hasLast_ = !history_.empty();
    if (hasLast_) last_ = history_.back();
}

bool Game::fiveAt(int x, int y, int player) const {
    static const int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    for (const auto& d : dirs) {
        int n = 1;
        for (int sign = -1; sign <= 1; sign += 2) {
            int i = x + d[0] * sign;
            int j = y + d[1] * sign;
            while (i >= 0 && i < SIZE && j >= 0 && j < SIZE && cells_[j][i] == player) {
                ++n;
                i += d[0] * sign;
                j += d[1] * sign;
            }
        }
        if (n >= 5) return true;
    }
    return false;
}

bool Game::boardFull() const { return static_cast<int>(history_.size()) >= SIZE * SIZE; }

}  // namespace gomoku
