// 与 Python 客户端之间的通信协议：一行一条 UTF-8 JSON，用 \n 分隔。
//
// 坐标：x 是列、y 是行，取 0~17；state 里的 board 是 board[y][x]。
//
// 客户端发来：
//   {"type":"new_game","player":1,"difficulty":3}   player 1=玩家执黑先手 2=执白后手
//   {"type":"move","x":2,"y":3}
//   {"type":"undo"}
//   {"type":"state"}
//
// 服务端返回：
//   {"type":"state", board, current_player, game_over, winner, undo_left,
//    message, thinking, human_player, ai_player, difficulty, move_count, last_move}
//   {"type":"error","message":"..."}
//
// 一次 new_game / move 最多回两条 state：先回 thinking=true 的那条（让界面
// 立刻画出玩家这一手），AI 算完再回最终状态。

#pragma once

#include <string>

#include <json.hpp>  // nlohmann/json 单头文件，放在 cpp/third_party/

using json = nlohmann::json;

namespace gomoku {

inline json errorMsg(const std::string& text) {
    return json{{"type", "error"}, {"message", text}};
}

// 拼成一整行，带上分隔用的换行
inline std::string encodeLine(const json& msg) { return msg.dump() + "\n"; }

}  // namespace gomoku
