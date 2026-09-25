#include "ai.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace gomoku {
namespace {

using Clock = std::chrono::steady_clock;

const int DIRS[4][2] = {{1, 1}, {1, 0}, {0, 1}, {1, -1}};

// 评估函数的取值范围到不了 1e7，所以这两个数一出现就等于是必胜 / 必败
const long long WIN = 1000000000000LL;
const long long INF = 1000000000000000LL;

const int ROOT_WIDTH = 14;   // 根节点展开的候选数
const int INNER_WIDTH = 10;  // 内层展开的候选数

struct Board {
    int c[SIZE][SIZE];
};

struct Cand {
    int x, y;
    long long score;
};

struct Ctx {
    int ai = BLACK;
    int human = WHITE;
    Clock::time_point deadline;
    long long nodes = 0;
    bool timeout = false;
};

bool inBoard(int x, int y) { return x >= 0 && x < SIZE && y >= 0 && y < SIZE; }

bool hasFive(const Board& b, int x, int y, int player) {
    for (const auto& d : DIRS) {
        int n = 1;
        for (int sign = -1; sign <= 1; sign += 2) {
            int i = x + d[0] * sign, j = y + d[1] * sign;
            while (inBoard(i, j) && b.c[j][i] == player) {
                ++n;
                i += d[0] * sign;
                j += d[1] * sign;
            }
        }
        if (n >= 5) return true;
    }
    return false;
}

// 原版 arnd：周围 3x3 里的棋子数
int neighbors(const Board& b, int x, int y) {
    int n = 0;
    for (int j = y - 1; j <= y + 1; ++j)
        for (int i = x - 1; i <= x + 1; ++i)
            if (inBoard(i, j) && b.c[j][i] != EMPTY) ++n;
    return n;
}

// 连子数 + 活端数 -> 价值，只用来给候选点排序
long long shapeValue(int n, int open) {
    if (n >= 5) return 1000000000LL;
    static const long long table[5][3] = {
        {0, 0, 0},
        {0, 20, 200},
        {0, 400, 4000},
        {0, 8000, 80000},
        {0, 100000, 1000000},
    };
    if (open > 2) open = 2;
    return table[n][open];
}

// 假设 player 下在 (x,y)，四个方向加起来能形成多少价值
long long pointValue(const Board& b, int x, int y, int player) {
    long long total = 0;
    for (const auto& d : DIRS) {
        int n = 1, open = 0;
        for (int sign = -1; sign <= 1; sign += 2) {
            int i = x + d[0] * sign, j = y + d[1] * sign;
            while (inBoard(i, j) && b.c[j][i] == player) {
                ++n;
                i += d[0] * sign;
                j += d[1] * sign;
            }
            if (inBoard(i, j) && b.c[j][i] == EMPTY) ++open;
        }
        total += shapeValue(n, open);
    }
    return total;
}

// 原版 get_val：四个方向各滑一遍长度 5 的窗口，按窗口里自己的子数加分。
// weight 是原版的附加值系数——AI 传 10、玩家传 80，所以 AI 更偏防守。
long long windowScore(const Board& b, int player, int weight) {
    const int foe = opponent(player);
    long long val = 0;

    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            for (const auto& d : DIRS) {
                int x = i, y = j, n = 0, tk = 0;
                for (int k = 0; k < 5; ++k) {
                    if (!inBoard(x, y) || b.c[y][x] == foe) {
                        n = 0;
                        break;
                    }
                    if (b.c[y][x] == player) {
                        ++n;
                        tk |= 1 << k;
                    }
                    x += d[0];
                    y += d[1];
                }

                switch (n) {
                    case 5: val += 800000000LL; break;
                    case 4: val += 1000 + 350LL * weight; break;
                    // tk == 0b01110 表示三子夹在窗口中间，两头还有空，也就是活三
                    case 3: val += (tk == 14) ? (300 + 600LL * weight) : (300 + 200LL * weight); break;
                    case 2: val += 3 + 2LL * weight; break;
                    case 1: val += 1 + 1LL * weight; break;
                    default: break;
                }
            }
        }
    }
    return val;
}

long long evaluate(const Board& b, const Ctx& ctx) {
    return windowScore(b, ctx.ai, 10) - windowScore(b, ctx.human, 80);
}

int collect(const Board& b, int mover, int foe, Cand* out, int limit) {
    int n = 0;
    for (int y = 0; y < SIZE; ++y) {
        for (int x = 0; x < SIZE; ++x) {
            if (b.c[y][x] != EMPTY || neighbors(b, x, y) == 0) continue;
            // 自己的棋型算双份，但“堵对手成五”也得排到前面去
            out[n++] = {x, y, pointValue(b, x, y, mover) * 2 + pointValue(b, x, y, foe)};
        }
    }

    auto cmp = [](const Cand& a, const Cand& c) { return a.score > c.score; };
    if (n > limit) {
        std::partial_sort(out, out + limit, out + n, cmp);
        n = limit;
    } else {
        std::sort(out, out + n, cmp);
    }
    return n;
}

long long minimax(Board& b, int depth, long long alpha, long long beta, bool maximize, int lastX,
                  int lastY, int lastPlayer, int width, Ctx& ctx) {
    if ((++ctx.nodes & 0x3FF) == 0 && Clock::now() >= ctx.deadline) ctx.timeout = true;
    if (ctx.timeout) return 0;  // 结果没意义，调用方会整层丢掉

    // 上一步已经成五，直接给终局分；用剩余深度微调，让 AI 更愿意早点取胜
    if (hasFive(b, lastX, lastY, lastPlayer)) {
        const long long s = WIN + depth;
        return lastPlayer == ctx.ai ? s : -s;
    }
    if (depth <= 0) return evaluate(b, ctx);

    const int mover = maximize ? ctx.ai : ctx.human;
    const int foe = maximize ? ctx.human : ctx.ai;

    Cand cands[SIZE * SIZE];
    const int n = collect(b, mover, foe, cands, width);
    if (n == 0) return evaluate(b, ctx);

    long long best = maximize ? -INF : INF;
    for (int i = 0; i < n; ++i) {
        b.c[cands[i].y][cands[i].x] = mover;
        const long long v =
            minimax(b, depth - 1, alpha, beta, !maximize, cands[i].x, cands[i].y, mover, width, ctx);
        b.c[cands[i].y][cands[i].x] = EMPTY;
        if (ctx.timeout) return 0;

        if (maximize) {
            best = std::max(best, v);
            alpha = std::max(alpha, best);
        } else {
            best = std::min(best, v);
            beta = std::min(beta, best);
        }
        if (alpha >= beta) break;
    }
    return best;
}

int budgetMs(int difficulty) {
    switch (difficulty) {
        case 1: return 150;
        case 2: return 500;
        case 3: return 1500;
        default: return 3000;
    }
}

bool boardEmpty(const Board& b) {
    for (int y = 0; y < SIZE; ++y)
        for (int x = 0; x < SIZE; ++x)
            if (b.c[y][x] != EMPTY) return false;
    return true;
}

}  // namespace

void chooseMove(const int board[SIZE][SIZE], int aiPlayer, int difficulty, int& outX, int& outY) {
    Board b;
    std::memcpy(b.c, board, sizeof(b.c));

    const int center = SIZE / 2 - 1;
    if (boardEmpty(b)) {
        outX = outY = center;
        return;
    }

    const int human = opponent(aiPlayer);
    Cand cands[SIZE * SIZE];
    const int n = collect(b, aiPlayer, human, cands, ROOT_WIDTH);
    if (n == 0) {
        outX = outY = center;
        for (int y = 0; y < SIZE; ++y) {
            for (int x = 0; x < SIZE; ++x) {
                if (b.c[y][x] == EMPTY) {
                    outX = x;
                    outY = y;
                    return;
                }
            }
        }
        return;
    }

    // 能一步成五就赢，对手能一步成五就堵
    for (int i = 0; i < n; ++i) {
        for (int who : {aiPlayer, human}) {
            b.c[cands[i].y][cands[i].x] = who;
            const bool five = hasFive(b, cands[i].x, cands[i].y, who);
            b.c[cands[i].y][cands[i].x] = EMPTY;
            if (five) {
                outX = cands[i].x;
                outY = cands[i].y;
                return;
            }
        }
    }

    // 迭代加深。每层算完把最好的着法挪到最前面，下一层能剪得更多；
    // 超时就丢掉没算完的这层，保留上一层的结果。
    Ctx ctx;
    ctx.ai = aiPlayer;
    ctx.human = human;
    ctx.deadline = Clock::now() + std::chrono::milliseconds(budgetMs(difficulty));

    outX = cands[0].x;
    outY = cands[0].y;

    for (int depth = 1; depth <= difficulty; ++depth) {
        for (int i = 1; i < n; ++i) {
            if (cands[i].x == outX && cands[i].y == outY) {
                std::swap(cands[0], cands[i]);
                break;
            }
        }

        long long best = -INF, alpha = -INF;
        int bestX = -1, bestY = -1;

        for (int i = 0; i < n; ++i) {
            b.c[cands[i].y][cands[i].x] = aiPlayer;
            const long long v =
                minimax(b, depth - 1, alpha, INF, false, cands[i].x, cands[i].y, aiPlayer,
                        INNER_WIDTH, ctx);
            b.c[cands[i].y][cands[i].x] = EMPTY;
            if (ctx.timeout) break;

            if (v > best) {
                best = v;
                bestX = cands[i].x;
                bestY = cands[i].y;
            }
            alpha = std::max(alpha, best);
        }

        if (ctx.timeout) break;
        if (bestX >= 0) {
            outX = bestX;
            outY = bestY;
        }
        if (best >= WIN) break;  // 已经算到必胜，不用再往深了
    }
}

}  // namespace gomoku
