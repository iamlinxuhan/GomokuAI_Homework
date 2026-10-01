#pragma once

#include "game.h"

namespace gomoku {

// 替 aiPlayer 选一个落点，写进 outX / outY
// difficulty 1~4 既当搜索深度用，也决定时间预算
void chooseMove(const int board[SIZE][SIZE], int aiPlayer, int difficulty, int& outX, int& outY);

}  // namespace gomoku
