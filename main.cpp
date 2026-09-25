//吝旭涵原创
#include<bits/stdc++.h>
#include<windows.h>
using namespace std;
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1:0)
POINT p;
HWND h = GetForegroundWindow();
int gamekunnan;
//全局变量
int gameplayer=1;
int qi_p[18][18];//棋盘、
int return_map[4][18][18];
int gamerule = 3; //游戏状态：1为失败，2为胜利，3为正在进行
int a, b; //ai对手下棋的x和y坐标
int fx[4][2] = { {1, 1}, {1, 0}, {0, 1}, {1, -1}};
int gamemode;//下棋先后手
int output=3;//剩余悔棋次数
int x, y;
int gamepalce;
int gamestartx,gamestarty;
int csh() { //初始化棋盘
	for (int i = 0; i < 18; i++) {
		for (int j = 0; j < 18; j++) {
			qi_p[i][j] = 0; //初始化
		}
	}
	return 0;
}
//清空缓存
int white_map(){
	for(int i=0;i<4;i++){
		for(int j=0;j<18;j++){
			for(int k=0;k<18;k++){
				return_map[i][j][k]=0;
			}
		}
	}
}
//将棋盘数据写入缓存
int move_the_map(){
	for(int i=1;i<4;i++){
		for(int j=0;j<18;j++){
			for(int k=0;k<18;k++){
				return_map[i-1][j][k]=return_map[i][j][k];
			}
		}
	}
	for(int i=0;i<18;i++){
		for(int j=0;j<18;j++){
			return_map[3][i][j]=qi_p[i][j];
		}
	}	
}
//悔棋后迁移棋盘数据
int if_move_the_map(){
	for(int i=0;i<18;i++){
		for(int j=0;j<18;j++){
			qi_p[i][j]=return_map[2][i][j];
		}
	}
	for(int i=3;i>=1;i--){
		for(int j=0;j<18;j++){
			for(int k=0;k<18;k++){
				return_map[i][j][k]=return_map[i-1][j][k];
			}
		}
	}
}
//转换坐标；四舍五入
int fouroutfivein() {
	float x1 = (p.y - 24) / 39;
	float y1 = (p.x - 24) / 39;
	int x2 = int(x1);
	int y2 = int(y1);
	if (x1 - x2 >= 0.5) {
		x = x2 + 1;
	} else {
		x = x2;
	}
	if (y1 - y2 >= 0.5) {
		y = y2 + 1;
	} else {
		y = y2;
	}
	return 0;
}
int getmouemmouve() {
	GetCursorPos(&p);
	ScreenToClient(h, &p);
	fouroutfivein();
	return 0;
}
void mouseleftkeydown() {
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);
	mode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(hStdin, mode);
	while (1) {
		if (KEY_DOWN(VK_LBUTTON)) {
			getmouemmouve();
			return;
		}
		Sleep(20);
	}
 
}
 
void clearScreen() {
	COORD coordScreen = { 0, 0 };
	SetConsoleCursorPosition(hConsole, coordScreen);
}
// 辅助函数，用于检查指定方向是否有连续n个棋子
bool checkDirection(int x, int y, int dx, int dy, int player, int n) {
	int count = 1;
	for (int i = 1; i < n; ++i) {
		int nx = x + i * dx;
		int ny = y + i * dy;
		if (nx >= 0 && nx < 18 && ny >= 0 && ny < 18 && qi_p[nx][ny] == player) {
			++count;
		} else {
			break;
		}
	}
	for (int i = 1; i < n; ++i) {
		int nx = x - i * dx;
		int ny = y - i * dy;
		if (nx >= 0 && nx < 18 && ny >= 0 && ny < 18 && qi_p[nx][ny] == player) {
			++count;
		} else {
			break;
		}
	}
	return count >= n;
}
void timesleep(int microseconds) {
	LARGE_INTEGER frequency;
	LARGE_INTEGER start, current;
	double elapsedMicroseconds;
	QueryPerformanceFrequency(&frequency);
	double waitTicks = static_cast<double>(microseconds) * frequency.QuadPart / 1e6;
	QueryPerformanceCounter(&start);
	do {
		QueryPerformanceCounter(&current);
		elapsedMicroseconds = static_cast<double>(current.QuadPart - start.QuadPart) * 1e6 / frequency.QuadPart;
	} while (elapsedMicroseconds < microseconds);
}
void Color(int colorpaint) {
	if (colorpaint == 0) {
		SetConsoleTextAttribute(hConsole, BACKGROUND_RED | BACKGROUND_GREEN | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	}
	if (colorpaint == 1) {
		SetConsoleTextAttribute(hConsole, BACKGROUND_RED | BACKGROUND_GREEN );
	}
	if (colorpaint == 2) {
		SetConsoleTextAttribute(hConsole, BACKGROUND_RED | BACKGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE);	
	}
	if (colorpaint == 3){
		SetConsoleTextAttribute(hConsole, BACKGROUND_GREEN | BACKGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_INTENSITY);
	}
}
int printj() {
	cout << "  ║   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   ║" << endl;
	return 0;
}
int screen() {
	cout << "      1   2   3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18 " << endl;
	cout << "  ╔═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╗" << endl;
	printj();
	for (int i = 0; i < 18; i++) {
		if (i <= 8) {
			cout << " ";
		}
		cout << i + 1;
		cout << "║─";
		for (int j = 0; j < 18; j++) {
			if (qi_p[i][j] == 0) {
				cout << "──┼─";
			}
 
			if (qi_p[i][j] == 1) {
				cout << "──";
				Color(1);
				cout << "●";
 
			}
			if (qi_p[i][j] == 2) {
				cout << "──";
				Color(0);
				cout << "●";
				Color(1);
			}
		}
		cout << "──╢" << endl;
		printj();
	}
	cout << "  ╚═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╝" << endl;
 
	return 0;
}
int arnd(int x, int y) {
	int cnt = 0;
	for (int i = x - 1; i <= x + 1; i++) {
		if (i >= 0 && i < 18) {
			for (int j = y - 1; j <= y + 1; j++) {
				if (j >= 0 && j < 18) {
					if (qi_p[i][j] > 0) cnt++;
				}
			}
		}
	}
	return cnt;
}
void get_val(int x, int y, int &val) {
	val = 0;
	for (int i = 0; i < 18; i++) {
		for (int j = 0; j < 18; j++) {
			for (int Fx = 0; Fx < 4; Fx++) {
				int xx = i, yy = j, tmp = 0, tk = 0;
				for (int k = 1; k <= 5; k++) {
					if (xx < 0 || xx >= 18 || yy < 0 || yy >= 18) {
						tmp = 0;
						break;
					}
					if (qi_p[xx][yy] == (x ^ 3)) {
						tmp = 0;
						break;
					}
					if (qi_p[xx][yy] == x) tmp++, tk += (1 << (k - 1));
					xx += fx[Fx][0], yy += fx[Fx][1];
				}
				switch (tmp) {
					case 5:
						val += 800000000;
						break;
					case 4:
						val += 1000 + 350 * y;
						break;
					case 3:
						val += (tk == 14) ? (300 + 600 * y) : (300 + 200 * y);
						break;
					case 2:
						val += 3 + 2 * y;
						break;
					case 1:
						val += 1 + y;
						break;
				}
			}
		}
	}
}
bool check_win(int player) {
	for (int i = 0; i < 18; ++i) {
		for (int j = 0; j < 18; ++j) {
			if (qi_p[i][j] == player) {
				if (checkDirection(i, j, 0, 1, player, 5)) return true;
				if (checkDirection(i, j, 1, 0, player, 5)) return true;
				if (checkDirection(i, j, 1, 1, player, 5)) return true;
				if (checkDirection(i, j, 1, -1, player, 5)) return true;
			}
		}
	}
	return false;
}
int evaluate(int ai_player) {
	int human_player = (ai_player == 1) ? 2 : 1;
	if (check_win(ai_player)) return INT_MAX;
	if (check_win(human_player)) return INT_MIN;
	int ai_score, human_score;
	get_val(ai_player, 10, ai_score);
	get_val(human_player, 80, human_score);
	return ai_score - human_score;
}
int minimax(int depth, int alpha, int beta, bool maximizingPlayer, int ai_player) {
	int human_player = (ai_player == 1) ? 2 : 1;
	int game_result = evaluate(ai_player);
	if (game_result == INT_MAX) return INT_MAX - depth;
	if (game_result == INT_MIN) return INT_MIN + depth;
	if (depth == 0) return game_result;
 
	vector<pair<int, int>> moves;
	for (int i = 0; i < 18; ++i) {
		for (int j = 0; j < 18; ++j) {
			if (qi_p[i][j] == 0 && arnd(i, j) > 0) {
				moves.push_back({i, j});
			}
		}
	}
	sort(moves.begin(), moves.end(), [](const pair<int, int>& a, const pair<int, int>& b) {
		return arnd(a.first, a.second) > arnd(b.first, b.second);
	});
 
	if (maximizingPlayer) {
		int maxEval = INT_MIN;
		for (auto& move : moves) {
			qi_p[move.first][move.second] = ai_player;
			int eval = minimax(depth - 1, alpha, beta, false, ai_player);
			qi_p[move.first][move.second] = 0;
			maxEval = max(maxEval, eval);
			alpha = max(alpha, eval);
			if (beta <= alpha) break;
		}
		return maxEval;
	} else {
		int minEval = INT_MAX;
		for (auto& move : moves) {
			qi_p[move.first][move.second] = human_player;
			int eval = minimax(depth - 1, alpha, beta, true, ai_player);
			qi_p[move.first][move.second] = 0;
			minEval = min(minEval, eval);
			beta = min(beta, eval);
			if (beta <= alpha) break;
		}
		return minEval;
	}
}
int AI_k(int ai) {
	int bestScore = INT_MIN;
	int ai_player = (gamemode == 0) ? 2 : 1;
	vector<pair<int, int>> bestMoves;
	vector<pair<int, int>> moves;
	for (int i = 0; i < 18; ++i) {
		for (int j = 0; j < 18; ++j) {
			if (qi_p[i][j] == 0 && arnd(i, j) > 0) {
				moves.push_back({i, j});
			}
		}
	}
	if (moves.empty()) {
		for (int i = 0; i < 18; ++i) {
			for (int j = 0; j < 18; ++j) {
				if (qi_p[i][j] == 0) {
					moves.push_back({i, j});
				}
			}
		}
	}
	sort(moves.begin(), moves.end(), [](const pair<int, int>& a, const pair<int, int>& b) {
		return arnd(a.first, a.second) > arnd(b.first, b.second);
	});
 
	int depth = ai;
	for (auto& move : moves) {
		qi_p[move.first][move.second] = ai_player;
		int currentScore = minimax(depth - 1, INT_MIN, INT_MAX, false, ai_player);
		qi_p[move.first][move.second] = 0;
 
		if (currentScore > bestScore) {
			bestScore = currentScore;
			bestMoves.clear();
			bestMoves.push_back(move);
		} else if (currentScore == bestScore) {
			bestMoves.push_back(move);
		}
	}
 
	if (!bestMoves.empty()) {
		int bestArnd = -1;
		pair<int, int> bestMove;
		for (auto& move : bestMoves) {
			int currentArnd = arnd(move.first, move.second);
			if (currentArnd > bestArnd) {
				bestArnd = currentArnd;
				bestMove = move;
			}
		}
		a = bestMove.first + 1;
		b = bestMove.second + 1;
	} else if (!moves.empty()) {
		a = moves[0].first + 1;
		b = moves[0].second + 1;
	} else {
		a = 9, b = 9;
	}
	return 0;
}
int AI(){
	AI_k(gamekunnan);
	return 0; 
}
int win_or_lose() {
	int player = (gamemode == 0) ? 1 : 2;
	int opponent = (player == 1) ? 2 : 1;
	for (int i = 0; i < 18; ++i) {
		for (int j = 0; j < 18; ++j) {
			if (qi_p[i][j] == player) {
				if (checkDirection(i, j, 0, 1, player, 5)) return 2;
				if (checkDirection(i, j, 1, 0, player, 5)) return 2;
				if (checkDirection(i, j, 1, 1, player, 5)) return 2;
				if (checkDirection(i, j, 1, -1, player, 5)) return 2;
			}
		}
	}
	bool fullBoard = true;
	for (int i = 0; i < 18; ++i) {
		for (int j = 0; j < 18; ++j) {
			if (qi_p[i][j] == 0) {
				fullBoard = false;
				break;
			}
		}
		if (!fullBoard) break;
	}
	if (fullBoard) return 0;
	return 3;
}
int return_moseleftkeydown(){
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);
	mode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(hStdin, mode);
	while (1) {
		if (KEY_DOWN(VK_LBUTTON)) {
			GetCursorPos(&p);
			ScreenToClient(h, &p);
			gamestartx=p.x;
			gamestarty=p.y;
			//cout<<gamestartx<<"   "<<gamestarty<<" :: ";
			if(gamestartx>=10 and gamestartx<=130 and gamestarty>=830 and gamestarty<=870){
				return 1;
			}else{
				return 0;
			}
		}
		Sleep(20);
	}
}
void print_return(){
	cout<<"剩余悔棋次数:"<<output<<"次。          "<<endl;
	cout<<"╔═══════════╗"<<endl;
	cout<<"║   悔  棋  ║"<<endl;;
	cout<<"╚═══════════╝"<<endl;
}
void returnmap(){
	output-=1;
	if_move_the_map();
	clearScreen();
	screen();
	print_return();
    Sleep(300);
}
int Game() {
	csh();
	while (true) {
		if (gamemode == 0) {
			clearScreen();
			screen();
			print_return();
			mouseleftkeydown();
			while (1) {
				if (x > 18 or y > 18 or x <= 0 or y <= 0) {
					if(return_moseleftkeydown() and output>0){
						returnmap();
					}else{
						clearScreen();
						screen();
						print_return();
					}
					
					mouseleftkeydown();
				} else if (qi_p[x - 1][y - 1] != 0) {
					clearScreen();
					screen();
					print_return();
					mouseleftkeydown();
				} else {
					break;
				}
			}
			if (gamemode == 0) {
				qi_p[x - 1][y - 1] = 1;
			}
			if (gamemode == 1) {
				qi_p[x - 1][y - 1] = 2;
			}
			if (win_or_lose() == 2) {
				gamerule = 2;
				return 0;
			}
			clearScreen();
			screen();
			Color(2);
			cout<<"ai正在思考中，时间可能较长"<<endl;
			cout<<"                                                   "<<endl;
			cout<<"                                                   "<<endl;
			cout<<"                                                   "<<endl;
			Color(1);
			AI();
			int ai_player = (gamemode == 0) ? 2 : 1;
			if (gamemode == 0) {
				qi_p[a - 1][b - 1] = 2;
			}
			if (gamemode == 1) {
				qi_p[a - 1][b - 1] = 1;
			}
			if (check_win(ai_player)) {  // 修改此处判断条件
				gamerule = 1;
				return 0;
			}
			move_the_map();
		}
		if (gamemode == 1) {
			clearScreen();
			screen();
			Color(2);
			cout<<"ai正在思考中，时间可能较长"<<endl;
			cout<<"                                                   "<<endl;
			cout<<"                                                   "<<endl;
			cout<<"                                                   "<<endl;
			Color(1);
			if(gameplayer!=1){
				AI();
			}else{
				gameplayer=2;
				a=9;
				b=9;
			}
			int ai_player = (gamemode == 0) ? 2 : 1;
			if (gamemode == 0) {
				qi_p[a - 1][b - 1] = 2;
			}
			if (gamemode == 1) {
				qi_p[a - 1][b - 1] = 1;
			}
			if (check_win(ai_player)) {  // 修改此处判断条件
				gamerule = 1;
				return 0;
			}
			move_the_map();
			clearScreen();
			screen();
			//cout<<"                                   "<<endl;
			print_return();
			mouseleftkeydown();
			while (1) {
			    if (x > 18 or y > 18 or x <= 0 or y <= 0) {
					if(return_moseleftkeydown() and output>0){
						returnmap();
					}else{
						clearScreen();
						screen();
						print_return();
					}								
					mouseleftkeydown();
				} else if (qi_p[x - 1][y - 1] != 0) {
					clearScreen();
					screen();
				    print_return();
					mouseleftkeydown();
				} else {
					break;
				}
			}
			if (gamemode == 0) {
				qi_p[x - 1][y - 1] = 1;
			}
			if (gamemode == 1) {
				qi_p[x - 1][y - 1] = 2;
			}
			if (win_or_lose() == 2) {
				gamerule = 2;
				return 0;
			}
 
		}
	}
}
void print_win_or_lose() {
	system("mode con cols=80 lines=45");
	if (gamerule == 1) {
		clearScreen();
		screen();
		SetConsoleTextAttribute(hConsole, BACKGROUND_RED | BACKGROUND_GREEN | FOREGROUND_RED );
		cout << "你输了                                         " << endl;
		Color(1);
	} else if (gamerule == 2) {
		clearScreen();
		screen();
		SetConsoleTextAttribute(hConsole, BACKGROUND_RED | BACKGROUND_GREEN | FOREGROUND_RED );
		cout << "你赢了                                        " << endl;;
		Color(1);
	}
}
int intro_moseleftkeydown(){
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);
	mode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(hStdin, mode);
	while (1) {
		if (KEY_DOWN(VK_LBUTTON)) {
			GetCursorPos(&p);
			ScreenToClient(h, &p);
			gamestartx=p.x;
			gamestarty=p.y;
			if(gamestartx>=540 and gamestartx<=680 and gamestarty>=430 and gamestarty<=470){
				return 1;
			}
		}
		Sleep(20);
	}
}
int intro() { //游戏开始界面
	cout << "╔═══════════════════════════════════════════════════════════════════╗" << endl;
	cout << "║                                                                   ║" << endl;
	cout << "║     ***欢迎运行五子棋游戏！***                                    ║" << endl;
	cout << "║                                                                   ║" << endl;
	cout << "║                                                                   ║" << endl;
	cout << "║                                                                   ║" << endl;
	cout << "║   【游戏规则如下：】                                              ║" << endl;
	cout << "║                                                                   ║" << endl;
	cout << "║   执黑棋者为先手                                                  ║" << endl;
	cout << "║                                                                   ║" << endl;
	cout << "║   在这个游戏里会有一个 18*18 的棋盘                               ║" << endl;
	cout << "║                                                                   ║" << endl;
	cout << "║   游戏目标是棋子在任意一个方向上连成5个，最先成功者胜利           ║" << endl;
	cout << "║                                                                   ║" << endl;
	cout << "║   每次点击要下棋的位置（点击位置必须在隔线交叉点上）              ║" << endl;
	cout << "║                                                                   ║" << endl;
	cout << "║   你将和ai进行对战                                                ║" << endl;
	cout << "║                                                                   ║" << endl;
	cout << "║                                             ";
	SetConsoleTextAttribute(hConsole,  BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY);
	cout << "【原创】吝旭涵";
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY);
	cout << "        ║" << endl;
	cout << "║   ";
	Color(3);
	cout <<"【注意：】难度越高，AI下棋速度越慢！";
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY);
	cout <<"                            ║" << endl;
	cout << "╚═══════════════════════════════════════════════════════════════════╝" << endl;
	cout << "                                                     ╔═════════════╗" << endl;
	cout << "                                                     ║ 开 始 游 戏 ║" << endl;
	cout << "                                                     ╚═════════════╝" << endl;
	if(intro_moseleftkeydown()==1){
		return 0;
	}
}
void gamestart1_moseleftkeydown(){
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);
	mode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(hStdin, mode);
	while (1) {
		if (KEY_DOWN(VK_LBUTTON)) {
			GetCursorPos(&p);
			ScreenToClient(h, &p);
			gamestartx=p.x;
			gamestarty=p.y;
			if(gamestartx>=10 and gamestartx<=120 and gamestarty>=30 and gamestarty<=70){
				gamemode=0;
				return;
			}else if(gamestartx>=160 and gamestartx<=270 and gamestarty>=30 and gamestarty<=70){
				gamemode=1;
				return;
			}
		}
		Sleep(20);
	}
}
void gamestart2_moseleftkeydown(){
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);
	mode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(hStdin, mode);
	while (1) {
		if (KEY_DOWN(VK_LBUTTON)) {
			GetCursorPos(&p);
			ScreenToClient(h, &p);
			gamestartx=p.x;
			gamestarty=p.y;
			if(gamestartx>=10 and gamestartx<=80 and gamestarty>=30 and gamestarty<=70){
				gamekunnan=1;
				return;
			}else if(gamestartx>=120 and gamestartx<=190 and gamestarty>=30 and gamestarty<=70){
				gamekunnan=2;
				return;
			}else if(gamestartx>=230 and gamestartx<=300 and gamestarty>=30 and gamestarty<=70){
				gamekunnan=3;
				return;
			}else if(gamestartx>=340 and gamestartx<=510 and gamestarty>=30 and gamestarty<=70){
				gamekunnan=4;
				return;
			}
		}
		Sleep(20);
	}
}
int start(){
	white_map();
	output=3;
	gameplayer=1;
	gamerule = 3;
	gamepalce=1;
	SetConsoleTextAttribute(hConsole, BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY);
	system("mode con cols=50 lines=5");
    system("cls");
    cout << "请玩家选择执棋子类型                " << endl;
    cout << "╔══════════╗"<<"   "<<"╔══════════╗"<<endl;
    cout << "║  黑 棋   ║"<<"   "<<"║  白 棋   ║"<<endl;;
    cout << "╚══════════╝"<<"   "<<"╚══════════╝"<<endl;
    gamestart1_moseleftkeydown();
    clearScreen();
    cout << "请玩家选择游戏难度                     " << endl;
	cout << "╔══════╗"<<"   "<<"╔══════╗"<<"   "<<"╔══════╗"<<"   "<<"╔══════╗"<<endl;
	cout << "║ 1 级 ║"<<"   "<<"║ 2 级 ║"<<"   "<<"║ 3 级 ║"<<"   "<<"║ 4 级 ║"<<endl;;
	cout << "╚══════╝"<<"   "<<"╚══════╝"<<"   "<<"╚══════╝"<<"   "<<"╚══════╝"<<endl;
	Sleep(300);
	gamestart2_moseleftkeydown();
	SetConsoleTextAttribute(hConsole, BACKGROUND_RED | BACKGROUND_GREEN );
	system("mode con cols=80 lines=45");
	return 0;
}
int moregame_moseleftkeydown(){
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);
	mode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(hStdin, mode);
	while (1) {
		if (KEY_DOWN(VK_LBUTTON)) {
			GetCursorPos(&p);
			ScreenToClient(h, &p);
			gamestartx=p.x;
			gamestarty=p.y;
			if(gamestartx>=10 and gamestartx<=130 and gamestarty>=830 and gamestarty<=870){
				return 1;
			}else if(gamestartx>=210 and gamestartx<=330 and gamestarty>=830 and gamestarty<=870){
				return 2;
			}
		}
		Sleep(20);
	}
}
int more_game() {
	cout << "╔═══════════╗"<<"       "<<"╔═══════════╗"<<endl;
	cout << "║  再来一局 ║"<<"       "<<"║   离  开  ║"<<endl;;
	cout << "╚═══════════╝"<<"       "<<"╚═══════════╝"<<endl;
	return moregame_moseleftkeydown();
}
int gamestart_loading() { //模拟加载
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY);
	system("mode con cols=70 lines=25");
	intro();
	clearScreen();
	SetConsoleTextAttribute(hConsole, BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY);
	system("mode con cols=30 lines=3");
	for (int i = 0; i <= 6; i++) {
		cout << "loading ";
		timesleep(90000);
		for (int j = 0; j < 3; j++) {
			cout << ".";
			timesleep(90000);
		}
		clearScreen();
	}
	return 0;
}
int main() {
	gamestart_loading();
	start();
	Game();
	print_win_or_lose();
	gamepalce=more_game();
	while (true) {
		if (gamepalce == 1) {
			start();
			Game();
			print_win_or_lose();
			gamepalce = more_game();
		} else if (gamepalce == 2) {
			break;
		}
	}
	return 0;
}