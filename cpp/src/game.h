// 棋盘状态机。只管规则，不管网络也不管界面。
// 坐标 x 为列、y 为行，内部存成 cells_[y][x]。
#pragma once

#include <string>
#include <vector>

#include "protocol.h"

namespace gomoku {

constexpr int SIZE = 18;
constexpr int UNDO_LIMIT = 3;

constexpr int EMPTY = 0;
constexpr int BLACK = 1;
constexpr int WHITE = 2;

inline int opponent(int p) { return p == BLACK ? WHITE : BLACK; }

struct Move {
    int x = 0;
    int y = 0;
    int player = EMPTY;
};

class Game {
public:
    Game() { reset(BLACK, 2); }

    void reset(int humanSide, int difficulty);

    // 返回空串表示成功，否则是给玩家看的中文提示
    std::string playHuman(int x, int y);
    std::string undo();

    // 轮到 AI 就落一子，返回是否真的落了
    bool playAi();
    bool aiToMove() const { return !over_ && turn_ == ai_; }

    int at(int x, int y) const { return cells_[y][x]; }
    int humanSide() const { return human_; }
    int undoLeft() const { return undoLeft_; }

    json toJson(bool thinking, const std::string& message) const;
    std::string describe() const;

private:
    void put(int x, int y, int player);
    void takeBack();
    bool fiveAt(int x, int y, int player) const;
    bool boardFull() const;

    int cells_[SIZE][SIZE] = {};
    int human_ = BLACK;
    int ai_ = WHITE;
    int turn_ = BLACK;
    int difficulty_ = 2;
    int undoLeft_ = UNDO_LIMIT;
    int winner_ = EMPTY;
    bool over_ = false;
    bool hasLast_ = false;
    Move last_;
    std::vector<Move> history_;
};

}  // namespace gomoku
