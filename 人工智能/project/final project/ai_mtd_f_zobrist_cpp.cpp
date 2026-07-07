#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <string> 
#include <cstdint>
#include <cstdlib>
#include <cstring>

using namespace std;
using namespace std::chrono;

// 基础配置与常量
const int BOARD_SIZE = 15;
const int EMPTY = 0;
const int BLACK = 1;
const int WHITE = 2;

// 棋型对应分数评估权重
const int SCORE_5 = 100000000;
const int SCORE_4 = 1000000;
const int SCORE_3 = 10000;
const int SCORE_2 = 100;
const int SCORE_1 = 10;

// 置换表大小：约 64MB (按位掩码快速取模)
const uint32_t TT_SIZE = 1 << 21; 
const uint32_t TT_MASK = TT_SIZE - 1;

// 置换表节点类型标志
const int TT_EMPTY = 0;
const int TT_EXACT = 1;
const int TT_LOWERBOUND = 2;
const int TT_UPPERBOUND = 3;

// Zobrist 随机哈希表，用于局面状态的 O(1) 更新
uint64_t ZOBRIST_TABLE[BOARD_SIZE][BOARD_SIZE][2];

// 搜索限制参数
double TIME_LIMIT = 15.0; 
int MAX_SEARCH_DEPTH = 8; 

// 数据结构定义：记录坐标与对应的启发式打分
struct Move {
    int r, c;
    int score;
    Move() : r(-1), c(-1), score(0) {}
    Move(int _r, int _c, int _s = 0) : r(_r), c(_c), score(_s) {}
};

// 插入排序：用于候选节点的 Move Ordering，加速剪枝
void sort_moves(Move arr[], int n) {
    for (int i = 1; i < n; i++) {
        Move key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j].score < key.score) { 
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// 置换表项结构
struct TTEntry {
    uint64_t hash;
    int depth;
    int flag;
    int score;
    Move best_move;
};

TTEntry* TT = nullptr;

// 初始化 Zobrist 哈希表，填充随机的 64 位整数
void init_zobrist() {
    mt19937_64 rng(42); 
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            ZOBRIST_TABLE[r][c][0] = rng(); 
            ZOBRIST_TABLE[r][c][1] = rng(); 
        }
    }
}

// AI 核心引擎类
class Agent {
private:
    steady_clock::time_point start_time;
    bool timeout_flag;
    long long node_count;

    // 扫描单线逻辑，识别连五或活四构成的绝对死局
    void scan_line_for_surrender(int line[], int n, int& surrender) {
        if (n < 5) return;
        for (int i = 0; i <= n - 5; ++i) {
            int op_cnt = 0;
            for (int j = 0; j < 5; ++j) if (line[i+j] == opponent) op_cnt++;
            if (op_cnt == 5) surrender++;
        }
        for (int i = 0; i <= n - 6; ++i) {
            if (line[i] == EMPTY && line[i+5] == EMPTY) {
                int op_cnt = 0, my_cnt = 0;
                for (int j = 1; j <= 4; ++j) {
                    if (line[i+j] == opponent) op_cnt++;
                    else if (line[i+j] == color) my_cnt++;
                }
                if (op_cnt == 4 && my_cnt == 0) surrender++;
            }
        }
    }

public:
    int color;
    int opponent;
    int reached_depth;

    Agent(int _color) : color(_color), opponent(3 - _color), reached_depth(0), timeout_flag(false), node_count(0) {}

    // 检测当前棋盘是否已面临无法挽回的绝杀
    bool check_surrender(int board[BOARD_SIZE][BOARD_SIZE]) {
        int surrender = 0;
        int line[BOARD_SIZE];
        
        for (int r = 0; r < BOARD_SIZE; ++r) {
            for (int c = 0; c < BOARD_SIZE; ++c) line[c] = board[r][c];
            scan_line_for_surrender(line, BOARD_SIZE, surrender);
        }
        for (int c = 0; c < BOARD_SIZE; ++c) {
            for (int r = 0; r < BOARD_SIZE; ++r) line[r] = board[r][c];
            scan_line_for_surrender(line, BOARD_SIZE, surrender);
        }
        for (int d = -BOARD_SIZE + 1; d < BOARD_SIZE; ++d) {
            int n = 0;
            for (int i = 0; i < BOARD_SIZE; ++i) {
                if (i - d >= 0 && i - d < BOARD_SIZE) line[n++] = board[i][i - d];
            }
            scan_line_for_surrender(line, n, surrender);
        }
        for (int d = -BOARD_SIZE + 1; d < BOARD_SIZE; ++d) {
            int n = 0;
            for (int i = 0; i < BOARD_SIZE; ++i) {
                if (BOARD_SIZE - 1 - i - d >= 0 && BOARD_SIZE - 1 - i - d < BOARD_SIZE) {
                    line[n++] = board[i][BOARD_SIZE - 1 - i - d];
                }
            }
            scan_line_for_surrender(line, n, surrender);
        }
        return surrender >= 1;
    }

    // 分数转换器
    inline int get_score(int count) {
        switch(count) {
            case 5: return SCORE_5;
            case 4: return SCORE_4;
            case 3: return SCORE_3;
            case 2: return SCORE_2;
            case 1: return SCORE_1;
            default: return 0;
        }
    }

    // 全局静态评估：计算 己方得分 - 对手得分
    int evaluate_board(int board[BOARD_SIZE][BOARD_SIZE], int cur_color) {
        int score = 0;
        int op_color = 3 - cur_color;
        
        for (int r = 0; r < BOARD_SIZE; ++r) {
            for (int c = 0; c < BOARD_SIZE; ++c) {
                if (c <= BOARD_SIZE - 5) {
                    int my_c = 0, op_c = 0;
                    for (int i = 0; i < 5; ++i) {
                        if (board[r][c+i] == cur_color) my_c++;
                        else if (board[r][c+i] == op_color) op_c++;
                    }
                    if (my_c > 0 && op_c == 0) score += get_score(my_c);
                    else if (op_c > 0 && my_c == 0) score -= get_score(op_c);
                }
                if (r <= BOARD_SIZE - 5) {
                    int my_c = 0, op_c = 0;
                    for (int i = 0; i < 5; ++i) {
                        if (board[r+i][c] == cur_color) my_c++;
                        else if (board[r+i][c] == op_color) op_c++;
                    }
                    if (my_c > 0 && op_c == 0) score += get_score(my_c);
                    else if (op_c > 0 && my_c == 0) score -= get_score(op_c);
                }
                if (r <= BOARD_SIZE - 5 && c <= BOARD_SIZE - 5) {
                    int my_c = 0, op_c = 0;
                    for (int i = 0; i < 5; ++i) {
                        if (board[r+i][c+i] == cur_color) my_c++;
                        else if (board[r+i][c+i] == op_color) op_c++;
                    }
                    if (my_c > 0 && op_c == 0) score += get_score(my_c);
                    else if (op_c > 0 && my_c == 0) score -= get_score(op_c);
                }
                if (r <= BOARD_SIZE - 5 && c >= 4) {
                    int my_c = 0, op_c = 0;
                    for (int i = 0; i < 5; ++i) {
                        if (board[r+i][c-i] == cur_color) my_c++;
                        else if (board[r+i][c-i] == op_color) op_c++;
                    }
                    if (my_c > 0 && op_c == 0) score += get_score(my_c);
                    else if (op_c > 0 && my_c == 0) score -= get_score(op_c);
                }
            }
        }
        return score;
    }

    // 单点启发式打分：用于评估尚未落子的空位价值
    int evaluate_point(int board[BOARD_SIZE][BOARD_SIZE], int r, int c, int cur_color) {
        if (board[r][c] != EMPTY) return -1000000000;
        int val = 0;
        int op_color = 3 - cur_color;
        
        // 声明为 static const 数组，避免在 evaluate_point 的每次调用中都在栈上复制数组，极大降低栈溢出风险。
        // 防止其在循环展开时于栈区疯狂复制数组引发栈溢出
        static const int dr[] = {1, 0, 1, 1};
        static const int dc[] = {0, 1, 1, -1};
        static const int steps[2] = {-1, 1};
        
        // 1. 攻防价值累加
        for (int d = 0; d < 4; ++d) {
            for (int i = -4; i <= 0; ++i) {
                int my_c = 0, op_c = 0;
                bool valid = true;
                for (int j = 0; j < 5; ++j) {
                    int nr = r + (i+j)*dr[d], nc = c + (i+j)*dc[d];
                    if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                        if (board[nr][nc] == cur_color) my_c++;
                        else if (board[nr][nc] == op_color) op_c++;
                    } else {
                        valid = false; break;
                    }
                }
                if (valid) {
                    if (op_c == 0) val += get_score(my_c + 1);
                    if (my_c == 0) {
                        if (op_c == 3) val += SCORE_4 * 1.5;
                        else val += get_score(op_c + 1);
                    }
                }
            }
        }
        
        // 2. 预见性惩罚：避免走出自杀步
        int predictive_penalty = 0;
        board[r][c] = cur_color;
        for (int d = 0; d < 4; ++d) {
            for (int s = 0; s < 2; ++s) {
                int step = steps[s];
                int nr = r + dr[d]*step, nc = c + dc[d]*step;
                if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE && board[nr][nc] == EMPTY) {
                    int op_count = 1;
                    for (int k = 1; k < 5; ++k) {
                        int nnr = nr + k*dr[d], nnc = nc + k*dc[d];
                        if (nnr >= 0 && nnr < BOARD_SIZE && nnc >= 0 && nnc < BOARD_SIZE && board[nnr][nnc] == op_color) op_count++;
                        else break;
                    }
                    for (int k = 1; k < 5; ++k) {
                        int nnr = nr - k*dr[d], nnc = nc - k*dc[d];
                        if (nnr >= 0 && nnr < BOARD_SIZE && nnc >= 0 && nnc < BOARD_SIZE && board[nnr][nnc] == op_color) op_count++;
                        else break;
                    }
                    if (op_count >= 4) predictive_penalty += 500000;
                    else if (op_count == 3) predictive_penalty += 20000;
                }
            }
        }
        board[r][c] = EMPTY;
        val -= predictive_penalty;
        
        // 3. 偏向中心位置
        val -= abs(r - BOARD_SIZE/2) + abs(c - BOARD_SIZE/2); 
        return val;
    }

    // 启发式生成候选点并进行 Beam Search 动态分支收束
    void get_candidates(int board[BOARD_SIZE][BOARD_SIZE], int depth, int cur_color, Move out_moves[], int& out_count) {
        Move candidates[225];
        int count = 0;
        bool has_piece = false;
        
        // 仅搜索周围2格范围内有棋子的空位（局部性剪枝）
        for (int r = 0; r < BOARD_SIZE; ++r) {
            for (int c = 0; c < BOARD_SIZE; ++c) {
                if (board[r][c] != EMPTY) {
                    has_piece = true; continue;
                }
                bool near = false;
                for (int i = max(0, r-2); i <= min(BOARD_SIZE-1, r+2); ++i) {
                    for (int j = max(0, c-2); j <= min(BOARD_SIZE-1, c+2); ++j) {
                        if (board[i][j] != EMPTY) { near = true; break; }
                    }
                    if (near) break;
                }
                if (near) {
                    int val = evaluate_point(board, r, c, cur_color);
                    candidates[count++] = Move(r, c, val);
                }
            }
        }
        
        // 空盘面默认落子天元
        if (!has_piece) {
            out_moves[0] = Move(BOARD_SIZE/2, BOARD_SIZE/2, 0);
            out_count = 1;
            return;
        }
        
        sort_moves(candidates, count);
        
        // 依据剩余深度调整搜索广度
        int limit = 4;
        if (depth >= 10) limit = 20;
        else if (depth >= 8) limit = 15;
        else if (depth >= 6) limit = 10;
        else if (depth >= 4) limit = 8;
        else if (depth == 3) limit = 5;
        
        if (count > 0 && candidates[0].score >= 10000000) limit = 2; 
        
        out_count = min(limit, count);
        for (int i = 0; i < out_count; ++i) {
            out_moves[i] = candidates[i];
        }
    }

    // Negamax 极小极大搜索算法：结合置换表缓存和 Alpha-Beta 剪枝
    int negamax(int board[BOARD_SIZE][BOARD_SIZE], uint64_t current_hash, int depth, int alpha, int beta, int cur_color) {
        node_count++;
        // 检查超时保护
        if ((node_count & 4095) == 0) {
            steady_clock::time_point now = steady_clock::now();
            double elapsed = duration<double>(now - start_time).count();
            if (elapsed > TIME_LIMIT) {
                timeout_flag = true;
                return 0; 
            }
        }

        int alpha_orig = alpha;
        
        // 读取置换表缓存 (Zobrist Hash)
        uint32_t tt_idx = current_hash & TT_MASK;
        TTEntry& tt_entry = TT[tt_idx];
        
        if (tt_entry.hash == current_hash && tt_entry.flag != TT_EMPTY && tt_entry.depth >= depth) {
            if (tt_entry.flag == TT_EXACT) return tt_entry.score;
            else if (tt_entry.flag == TT_LOWERBOUND) alpha = max(alpha, tt_entry.score);
            else if (tt_entry.flag == TT_UPPERBOUND) beta = min(beta, tt_entry.score);
            if (alpha >= beta) return tt_entry.score;
        }

        int score = evaluate_board(board, cur_color);
        if (depth == 0 || abs(score) >= 10000000) return score;

        Move candidates[25]; 
        int num_candidates = 0;
        get_candidates(board, depth, cur_color, candidates, num_candidates);

        if (num_candidates == 0) return score;

        // 利用置换表中的最佳历史步进行最高优排序 (Move Ordering)
        if (tt_entry.hash == current_hash && tt_entry.flag != TT_EMPTY) {
            for (int i = 0; i < num_candidates; ++i) {
                if (candidates[i].r == tt_entry.best_move.r && candidates[i].c == tt_entry.best_move.c) {
                    swap(candidates[0], candidates[i]);
                    break;
                }
            }
        }

        int best_val = -1000000000;
        Move best_move = candidates[0];

        // 遍历分支
        for (int i = 0; i < num_candidates; ++i) {
            const Move& move = candidates[i];
            board[move.r][move.c] = cur_color;
            uint64_t next_hash = current_hash ^ ZOBRIST_TABLE[move.r][move.c][cur_color - 1];
            
            // 递归向下搜：取负返回值，且交换并取反 Alpha 和 Beta
            int val = -negamax(board, next_hash, depth - 1, -beta, -alpha, 3 - cur_color);
            
            board[move.r][move.c] = EMPTY;
            
            if (timeout_flag) return 0; 
            
            if (val > best_val) {
                best_val = val;
                best_move = move;
            }
            alpha = max(alpha, val);
            if (alpha >= beta) break;
        }

        // 写入计算结果到置换表
        if (!timeout_flag) {
            tt_entry.hash = current_hash;
            tt_entry.depth = depth;
            tt_entry.score = best_val;
            tt_entry.best_move = best_move;
            if (best_val <= alpha_orig) tt_entry.flag = TT_UPPERBOUND;
            else if (best_val >= beta) tt_entry.flag = TT_LOWERBOUND;
            else tt_entry.flag = TT_EXACT;
        }

        return best_val;
    }

    // MTD(f) 零宽探测：借助不断的 Negamax 进行边界逼近，找出精确值
    Move mtdf(int board[BOARD_SIZE][BOARD_SIZE], uint64_t current_hash, int& first_f, int depth, int cur_color) {
        int g = first_f;
        int upperbound = 1000000000;
        int lowerbound = -1000000000;
        Move best_move;
        
        while (lowerbound < upperbound) {
            int beta = max(g, lowerbound + 1);
            g = negamax(board, current_hash, depth, beta - 1, beta, cur_color);
            if (timeout_flag) break;
            
            // 提取被 TT 记录的最佳走步
            uint32_t tt_idx = current_hash & TT_MASK;
            if (TT[tt_idx].hash == current_hash && TT[tt_idx].flag != TT_EMPTY) {
                best_move = TT[tt_idx].best_move;
            }
            
            if (g < beta) upperbound = g;
            else lowerbound = g;
        }
        first_f = g;
        return best_move;
    }

    // 迭代加深框架：逐层加深限制搜索
    Move time_bounded_search(int board[BOARD_SIZE][BOARD_SIZE], uint64_t current_hash, int cur_color) {
        start_time = steady_clock::now();
        timeout_flag = false;
        node_count = 0;
        int f = 0;
        Move best_move_overall;
        
        for (int d = 1; d <= MAX_SEARCH_DEPTH; ++d) {
            Move move = mtdf(board, current_hash, f, d, cur_color);
            if (timeout_flag) break; 
            
            best_move_overall = move;
            reached_depth = d; 
            
            if (abs(f) >= 10000000) break;
        }
        return best_move_overall;
    }
};

// 控制台游戏与 UI 引擎
class Game {
    int board[BOARD_SIZE][BOARD_SIZE];
    vector<pair<int, int>> history;
    int human_color;
    int ai_color;
    int turn;
    bool over;
    uint64_t zobrist_hash;
    Agent* agent;

public:
    Game() {
        // 必须在分配前置空 agent，否则如果系统分配的堆内存含有脏数据，
        // reset() 中 if (agent) delete agent; 会直接崩溃
        agent = nullptr; 
        
        init_zobrist();
        // 初始化置换表内存
        TT = (TTEntry*)malloc(TT_SIZE * sizeof(TTEntry));
        if (TT == nullptr) {
            cout << "Fatal Error: Failed to allocate memory for TT!" << endl;
            exit(1);
        }
        memset(TT, 0, TT_SIZE * sizeof(TTEntry));
        
        reset(BLACK);
    }
    
    ~Game() {
        free(TT);
        if (agent) delete agent;
    }

    // 初始化并清空棋局
    void reset(int h_color) {
        for(int r = 0; r < BOARD_SIZE; ++r) {
            for(int c = 0; c < BOARD_SIZE; ++c) {
                board[r][c] = EMPTY;
            }
        }
        human_color = h_color;
        ai_color = 3 - h_color;
        turn = BLACK;
        over = false;
        history.clear();
        zobrist_hash = 0;
        
        if (agent) delete agent;
        agent = new Agent(ai_color);
    }

    // 绘制控制台 UI，打印局面
    void draw_ui() {
        cout << "\n================= GOMOKU AI (C++ MTD(f) Engine) =================" << endl;
        cout << "  ";
        for (int i = 0; i < BOARD_SIZE; ++i) cout << setw(2) << i % 10;
        cout << "      [COMMANDS MENU]" << endl;
        
        for (int r = 0; r < BOARD_SIZE; ++r) {
            cout << setw(2) << r % 10 << " ";
            for (int c = 0; c < BOARD_SIZE; ++c) {
                if (board[r][c] == BLACK) cout << "X ";
                else if (board[r][c] == WHITE) cout << "O ";
                else {
                    if (!history.empty() && history.back().first == r && history.back().second == c) cout << "* ";
                    else cout << ". ";
                }
            }
            if (r == 1) cout << "    -> Input 'r c' to Play (e.g. 7 7)";
            else if (r == 3) cout << "    -> Input 'u' to Undo Move";
            else if (r == 5) cout << "    -> Input 'b' to Play Black (Restart)";
            else if (r == 6) cout << "    -> Input 'w' to Play White (Restart)";
            else if (r == 9) cout << "    [Game Status]: " << (over ? "Game Over!" : (turn == human_color ? "Your Turn" : "AI Thinking..."));
            else if (r == 10) cout << "    [Last Search Depth]: " << agent->reached_depth;
            cout << endl;
        }
        cout << "=================================================================" << endl;
    }

    // 棋步处理：更新状态并计算哈希增量
    void process_move(int r, int c, int color) {
        board[r][c] = color;
        history.push_back(make_pair(r, c));
        zobrist_hash ^= ZOBRIST_TABLE[r][c][color - 1];
        turn = 3 - color;
    }

    // 悔棋处理：回滚历史并修复哈希
    void undo() {
        if (history.empty() || over) return;
        
        int r = history.back().first;
        int c = history.back().second;
        board[r][c] = EMPTY;
        zobrist_hash ^= ZOBRIST_TABLE[r][c][turn - 1];
        history.pop_back();
        turn = 3 - turn;
        
        while (!history.empty() && turn != human_color) {
            int rr = history.back().first;
            int cc = history.back().second;
            board[rr][cc] = EMPTY;
            zobrist_hash ^= ZOBRIST_TABLE[rr][cc][turn - 1];
            history.pop_back();
            turn = 3 - turn;
        }
        over = false;
    }

    // 主交互引擎
    void run() {
        while (true) {
            draw_ui();
            
            // AI 回合执行
            if (!over && turn == ai_color) {
                if (agent->check_surrender(board)) {
                    cout << "\n>>> AI Surrendered! You WIN! <<<\n";
                    over = true;
                } else {
                    cout << "\nAI is thinking (Depth limit: " << MAX_SEARCH_DEPTH << ")... Please wait." << endl;
                    Move move = agent->time_bounded_search(board, zobrist_hash, ai_color);
                    if (move.r != -1) {
                        process_move(move.r, move.c, ai_color);
                    } else {
                        over = true;
                    }
                }
                continue; 
            }

            // 人类回合执行与指令解析
            cout << "\nYour move / command: ";
            string cmd;
            if (!(cin >> cmd)) break;
            
            if (cmd == "u" || cmd == "U") {
                undo();
            } else if (cmd == "b" || cmd == "B") {
                reset(BLACK);
            } else if (cmd == "w" || cmd == "W") {
                reset(WHITE);
            } else {
                try {
                    int r = stoi(cmd);
                    int c;
                    cin >> c;
                    if (!over && r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && board[r][c] == EMPTY) {
                        process_move(r, c, human_color);
                    } else {
                        cout << "Invalid move!" << endl;
                    }
                } catch (...) {
                    cout << "Unknown command!" << endl;
                }
            }
        }
    }
};

int main() {
    try {
        Game* game = new Game();
        game->run();
        delete game;
    } catch(const exception& e) {
        cout << "Error: " << e.what() << endl;
    }
    return 0;
}