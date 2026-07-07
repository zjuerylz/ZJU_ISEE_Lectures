import pygame
import sys
import math
import random
import gc


# 基础配置与常量
BOARD_SIZE = 15
CELL_SIZE = 45
MARGIN = 40
SIDEBAR_WIDTH = 250
WIDTH = BOARD_SIZE * CELL_SIZE + MARGIN * 2 + SIDEBAR_WIDTH
HEIGHT = BOARD_SIZE * CELL_SIZE + MARGIN * 2

COLOR_BOARD = (220, 179, 123)
COLOR_GRID = (60, 40, 20)
COLOR_SIDEBAR = (45, 45, 48)
COLOR_BTN = (70, 70, 75)
COLOR_BTN_HOVER = (90, 90, 95)
COLOR_TEXT = (240, 240, 240)

EMPTY = 0
BLACK = 1 
WHITE = 2 

SCORES = {
    5: 100000000, 
    4: 1000000,   
    3: 10000,     
    2: 100,       
    1: 10,        
    0: 0
}

# MTD(f) 核心组件：Zobrist Hashing (佐布里斯特散列)
# 为棋盘上的每一个位置的每一种颜色(黑/白)分配一个 64 位的随机整数。
random.seed(42) # 固定种子，保证每次运行生成的哈希表一致
ZOBRIST_TABLE = [[[random.getrandbits(64) for _ in range(2)] for _ in range(BOARD_SIZE)] for _ in range(BOARD_SIZE)]

# 置换表 Flag 定义
TT_EXACT = 0       # 精确值
TT_LOWERBOUND = 1  # 下界（被 Beta 剪枝）
TT_UPPERBOUND = 2  # 上界（被 Alpha 剪枝）

class Agent:
    def __init__(self, color):
        self.color = color
        self.opponent = 3 - color
        # 全局置换表 (Transposition Table)
        # 用内存换速度，装载上千万个推演过的棋局
        self.tt = {}

    def check_surrender(self, board):
        """实盘绝杀判定"""
        surrender = 0
        def scan_line(line):
            nonlocal surrender
            length = len(line)
            if length < 5: return
            for i in range(length - 4):
                if line[i:i+5].count(self.opponent) == 5: surrender += 1
            for i in range(length - 5):
                win6 = line[i:i+6]
                if win6[0] == EMPTY and win6[5] == EMPTY and win6.count(self.opponent) == 4 and win6.count(self.color) == 0:
                    surrender += 1
                    
        for r in range(BOARD_SIZE): scan_line([board[r][c] for c in range(BOARD_SIZE)])
        for c in range(BOARD_SIZE): scan_line([board[r][c] for r in range(BOARD_SIZE)])
        for d in range(-BOARD_SIZE+1, BOARD_SIZE):
            scan_line([board[i][i-d] for i in range(BOARD_SIZE) if 0 <= i-d < BOARD_SIZE])
        for d in range(-BOARD_SIZE+1, BOARD_SIZE):
            scan_line([board[i][BOARD_SIZE-1-i-d] for i in range(BOARD_SIZE) if 0 <= BOARD_SIZE-1-i-d < BOARD_SIZE])
            
        return surrender >= 1

    def evaluate_board(self, board, current_color):
        """
        Negamax 要求评估函数是对称的：返回 (当前玩家得分 - 对手得分)
        """
        score = 0
        op_color = 3 - current_color
        
        # 扫描四个方向累加估值
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                if c <= BOARD_SIZE - 5:
                    my_c, op_c = 0, 0
                    for i in range(5):
                        p = board[r][c+i]
                        if p == current_color: my_c += 1
                        elif p == op_color: op_c += 1
                    if my_c > 0 and op_c == 0: score += SCORES[my_c]
                    elif op_c > 0 and my_c == 0: score -= SCORES[op_c]
                
                if r <= BOARD_SIZE - 5:
                    my_c, op_c = 0, 0
                    for i in range(5):
                        p = board[r+i][c]
                        if p == current_color: my_c += 1
                        elif p == op_color: op_c += 1
                    if my_c > 0 and op_c == 0: score += SCORES[my_c]
                    elif op_c > 0 and my_c == 0: score -= SCORES[op_c]
                
                if r <= BOARD_SIZE - 5 and c <= BOARD_SIZE - 5:
                    my_c, op_c = 0, 0
                    for i in range(5):
                        p = board[r+i][c+i]
                        if p == current_color: my_c += 1
                        elif p == op_color: op_c += 1
                    if my_c > 0 and op_c == 0: score += SCORES[my_c]
                    elif op_c > 0 and my_c == 0: score -= SCORES[op_c]
                
                if r <= BOARD_SIZE - 5 and c >= 4:
                    my_c, op_c = 0, 0
                    for i in range(5):
                        p = board[r+i][c-i]
                        if p == current_color: my_c += 1
                        elif p == op_color: op_c += 1
                    if my_c > 0 and op_c == 0: score += SCORES[my_c]
                    elif op_c > 0 and my_c == 0: score -= SCORES[op_c]
                    
        return score

    def evaluate_point(self, board, r, c, current_color):
        # 启发式单点粗估算法：针对单一空位进行打分
        if board[r][c] != EMPTY: return -math.inf
        val = 0
        op_color = 3 - current_color
        directions = [(1,0), (0,1), (1,1), (1,-1)]
        
        # 1. 计算进攻与防守价值
        for dr, dc in directions:
            for i in range(-4, 1):
                my_c, op_c = 0, 0
                valid = True
                for j in range(5):
                    nr, nc = r + (i+j)*dr, c + (i+j)*dc
                    if 0 <= nr < BOARD_SIZE and 0 <= nc < BOARD_SIZE:
                        p = board[nr][nc]
                        if p == current_color: my_c += 1
                        elif p == op_color: op_c += 1
                    else:
                        valid = False; break
                if valid:
                    if op_c == 0: val += SCORES[my_c + 1]
                    if my_c == 0: 
                        if op_c == 3: val += SCORES[4] * 1.5 
                        else: val += SCORES[op_c + 1]
                        
        # 2. 计算被利用为绝杀的危险程度（预见性惩罚）
        predictive_penalty = 0
        board[r][c] = current_color 
        for dr, dc in directions:
            for step in [-1, 1]:
                nr, nc = r + dr*step, c + dc*step
                if 0 <= nr < BOARD_SIZE and 0 <= nc < BOARD_SIZE and board[nr][nc] == EMPTY:
                    op_count = 1
                    for k in range(1, 5):
                        nnr, nnc = nr + k*dr, nc + k*dc
                        if 0 <= nnr < BOARD_SIZE and 0 <= nnc < BOARD_SIZE and board[nnr][nnc] == op_color: op_count += 1
                        else: break
                    for k in range(1, 5):
                        nnr, nnc = nr - k*dr, nc - k*dc
                        if 0 <= nnr < BOARD_SIZE and 0 <= nnc < BOARD_SIZE and board[nnr][nnc] == op_color: op_count += 1
                        else: break
                    if op_count >= 4: predictive_penalty += 500000 
                    elif op_count == 3: predictive_penalty += 20000  
        board[r][c] = EMPTY 
        val -= predictive_penalty
        val -= abs(r - BOARD_SIZE//2) + abs(c - BOARD_SIZE//2)
        return val

    def get_candidates(self, board, depth, current_color):
        # 启发式生成并收束候选点
        candidates = []
        has_piece = False
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                if board[r][c] != EMPTY:
                    has_piece = True; continue
                near = False
                for i in range(max(0, r-2), min(BOARD_SIZE, r+3)):
                    for j in range(max(0, c-2), min(BOARD_SIZE, c+3)):
                        if board[i][j] != EMPTY:
                            near = True; break
                    if near: break
                if near:
                    val = self.evaluate_point(board, r, c, current_color)
                    candidates.append((val, r, c))
                    
        if not has_piece: return [(BOARD_SIZE//2, BOARD_SIZE//2)]
        
        candidates.sort(key=lambda x: x[0], reverse=True)
        
        # MTD(f) 加持下，收束可以稍微放宽，因为零宽搜索极快
        if depth >= 6: limit = 15
        elif depth >= 4: limit = 10
        elif depth == 3: limit = 8
        elif depth == 2: limit = 5
        else: limit = 3
        
        if candidates and candidates[0][0] >= 1e7: limit = 2
        return [(r, c) for _, r, c in candidates[:limit]] 

    # Negamax (带置换表的极简 Alpha-Beta)
    def negamax(self, board, current_hash, depth, alpha, beta, color):
        alpha_orig = alpha
        
        # 1. 查询内存缓存 (Zobrist Hash Lookup)
        tt_entry = self.tt.get(current_hash)
        if tt_entry and tt_entry[2] >= depth:
            tt_val, tt_flag, tt_move = tt_entry[0], tt_entry[1], tt_entry[3]
            if tt_flag == TT_EXACT: return tt_val, tt_move
            elif tt_flag == TT_LOWERBOUND: alpha = max(alpha, tt_val)
            elif tt_flag == TT_UPPERBOUND: beta = min(beta, tt_val)
            if alpha >= beta: return tt_val, tt_move

        score = self.evaluate_board(board, color)
        if depth == 0 or abs(score) >= 10000000:
            return score, None

        candidates = self.get_candidates(board, depth, color)
        if not candidates: return score, None

        # 如果缓存中记录了这个局面曾经的最佳走法，把它排在最前面(极大提升剪枝率)
        if tt_entry and tt_entry[3] in candidates:
            candidates.remove(tt_entry[3])
            candidates.insert(0, tt_entry[3])

        best_val = -math.inf
        best_move = candidates[0]

        for move in candidates:
            r, c = move
            board[r][c] = color
            # O(1) 更新哈希指纹，不需要深拷贝棋盘
            next_hash = current_hash ^ ZOBRIST_TABLE[r][c][color - 1]
            
            # Negamax 核心：向下传参时，交换并取反 alpha 和 beta，返回值取反
            val, _ = self.negamax(board, next_hash, depth - 1, -beta, -alpha, 3 - color)
            val = -val
            
            board[r][c] = EMPTY # 回溯
            
            if val > best_val:
                best_val = val
                best_move = move
                
            alpha = max(alpha, val)
            if alpha >= beta: break # 零宽剪枝
                
        # 2. 将计算结果存入全局缓存
        if best_val <= alpha_orig: flag = TT_UPPERBOUND
        elif best_val >= beta: flag = TT_LOWERBOUND
        else: flag = TT_EXACT
            
        # (score, flag, depth, best_move)
        self.tt[current_hash] = (best_val, flag, depth, best_move)
        return best_val, best_move

  
    # MTD(f) 零宽探测
    def mtdf(self, board, current_hash, first_f, depth, color):
        g = first_f
        upperbound = math.inf
        lowerbound = -math.inf
        best_move = None
        
        while lowerbound < upperbound:
            # 永远只用长度为 1 的窗口 [beta-1, beta] 去探测
            beta = max(g, lowerbound + 1)
            g, move = self.negamax(board, current_hash, depth, beta - 1, beta, color)
            
            if move is not None: best_move = move
                
            if g < beta:
                upperbound = g # 猜高了，调低上界
            else:
                lowerbound = g # 猜低了，调高下界
                
        return g, best_move

    # Iterative Deepening (迭代加深)

    def iterative_deepening(self, board, current_hash, max_depth, color):
        """
        滚雪球式搜索：从 Depth 1 搜到 Max_Depth。
        将上一层的精确得分作为下一层 MTD(f) 的初始猜测值 f，极大提升搜索效率。
        """
        f = 0
        best_move = None
        
        # 由于我们拥有几十万个节点的庞大 Hash 表，
        # 在思考期间必须关闭 Python 的垃圾回收，否则会导致画面严重卡顿冻结！
        gc.disable()
        
        for d in range(1, max_depth + 1):
            f, move = self.mtdf(board, current_hash, f, d, color)
            if move:
                best_move = move
            # 一旦发现绝杀或被绝杀，不需要再往深搜了，直接返回
            if abs(f) >= 10000000:
                break
                
        gc.enable() # 思考结束，恢复垃圾回收
        return best_move


class Game:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((WIDTH, HEIGHT))
        pygame.display.set_caption("AI Gomoku - MTD(f) & Zobrist Edition")
        self.font = pygame.font.Font(None, 24) 
        self.big_font = pygame.font.Font(None, 28)
        self.huge_font = pygame.font.Font(None, 80)
        
        self.reset(BLACK) 

    def reset(self, human_color):
        # 初始化清空逻辑
        self.board = [[EMPTY for _ in range(BOARD_SIZE)] for _ in range(BOARD_SIZE)]
        self.human_color = human_color
        self.ai_color = 3 - human_color
        self.turn = BLACK
        self.over = False
        self.ai_surrendered = False
        self.winner = EMPTY
        self.agent = Agent(self.ai_color)
        self.history = []
        self.last_move = None
        # 初始化空棋盘的 Hash 值为 0
        self.zobrist_hash = 0

    def undo(self):
        # 智能悔棋回滚
        if not self.history: return
        if not (self.turn == self.human_color or self.over): return
            
        self.history.pop()
        while self.history:
            next_turn = WHITE if len(self.history) % 2 != 0 else BLACK
            if next_turn == self.human_color: break
            self.history.pop()
            
        self.rebuild_board()

    def rebuild_board(self):
        # 复原棋盘并同步 O(1) 计算还原哈希
        self.board = [[EMPTY for _ in range(BOARD_SIZE)] for _ in range(BOARD_SIZE)]
        current_turn = BLACK
        self.last_move = None
        self.ai_surrendered = False
        self.zobrist_hash = 0 # 重置 Hash
        
        for r, c in self.history:
            self.board[r][c] = current_turn
            self.last_move = (r, c)
            # 通过异或重建盘面指纹
            self.zobrist_hash ^= ZOBRIST_TABLE[r][c][current_turn - 1]
            current_turn = 3 - current_turn
            
        self.turn = current_turn 
        self.over = False
        self.winner = EMPTY

    def draw_stone(self, r, c, color):
        x, y = MARGIN + c * CELL_SIZE, MARGIN + r * CELL_SIZE
        pygame.draw.circle(self.screen, (40, 40, 40), (x+2, y+2), CELL_SIZE//2 - 2)
        main_color = (20, 20, 20) if color == BLACK else (240, 240, 240)
        pygame.draw.circle(self.screen, main_color, (x, y), CELL_SIZE//2 - 2)
        high_color = (80, 80, 80) if color == BLACK else (255, 255, 255)
        pygame.draw.circle(self.screen, high_color, (x-4, y-4), CELL_SIZE//6)

    def draw_ui(self):
        # 绘制主界面与棋盘线格
        self.screen.fill(COLOR_BOARD)
        for i in range(BOARD_SIZE):
            pygame.draw.line(self.screen, COLOR_GRID, (MARGIN, MARGIN+i*CELL_SIZE), (MARGIN+(BOARD_SIZE-1)*CELL_SIZE, MARGIN+i*CELL_SIZE), 1)
            pygame.draw.line(self.screen, COLOR_GRID, (MARGIN+i*CELL_SIZE, MARGIN), (MARGIN+i*CELL_SIZE, MARGIN+(BOARD_SIZE-1)*CELL_SIZE), 1)
        
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                if self.board[r][c] != EMPTY:
                    self.draw_stone(r, c, self.board[r][c])
        
        if self.last_move:
            r, c = self.last_move
            pygame.draw.rect(self.screen, (255, 0, 0), (MARGIN+c*CELL_SIZE-5, MARGIN+r*CELL_SIZE-5, 10, 10), 2)

        if self.ai_surrendered:
            overlay = pygame.Surface((WIDTH - SIDEBAR_WIDTH, HEIGHT))
            overlay.set_alpha(150)
            overlay.fill((50, 50, 50))
            self.screen.blit(overlay, (0, 0))
            
            win_txt = self.huge_font.render("YOU WIN", True, (255, 215, 0))
            win_rect = win_txt.get_rect(center=((WIDTH - SIDEBAR_WIDTH)//2, HEIGHT//2 - 20))
            self.screen.blit(win_txt, win_rect)
            
            sub_txt = self.big_font.render("AI Surrendered!", True, (255, 255, 255))
            sub_rect = sub_txt.get_rect(center=((WIDTH - SIDEBAR_WIDTH)//2, HEIGHT//2 + 40))
            self.screen.blit(sub_txt, sub_rect)

        sidebar_rect = pygame.Rect(WIDTH - SIDEBAR_WIDTH, 0, SIDEBAR_WIDTH, HEIGHT)
        pygame.draw.rect(self.screen, COLOR_SIDEBAR, sidebar_rect)
        
        if self.ai_surrendered:
            status = "AI Gave Up!"
        elif self.over:
            status = "Game Over"
        else:
            status = "AI Thinking..." if self.turn == self.ai_color else "Your Turn"
            
        txt = self.big_font.render(status, True, COLOR_TEXT)
        self.screen.blit(txt, (WIDTH - SIDEBAR_WIDTH + 20, 50))

        if self.over and not self.ai_surrendered:
            win_txt = "Winner: " + ("Black" if self.winner == BLACK else "White")
            self.screen.blit(self.big_font.render(win_txt, True, (255, 200, 0)), (WIDTH - SIDEBAR_WIDTH + 20, 90))

        self.btn_black = self.draw_button("Restart (Play Black)", 200)
        self.btn_white = self.draw_button("Restart (Play White)", 270)
        self.btn_undo  = self.draw_button("Undo Move", 340)

    def draw_button(self, text, y_pos):
        mouse_pos = pygame.mouse.get_pos()
        rect = pygame.Rect(WIDTH - SIDEBAR_WIDTH + 20, y_pos, 210, 45)
        color = COLOR_BTN_HOVER if rect.collidepoint(mouse_pos) else COLOR_BTN
        pygame.draw.rect(self.screen, color, rect, border_radius=8)
        txt = self.font.render(text, True, COLOR_TEXT)
        self.screen.blit(txt, (rect.centerx - txt.get_width()//2, rect.centery - txt.get_height()//2))
        return rect

    def check_win(self):
        # 实时连五胜负判定
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                v = self.board[r][c]
                if v == EMPTY: continue
                if c <= BOARD_SIZE-5 and all(self.board[r][c+i] == v for i in range(5)): return v
                if r <= BOARD_SIZE-5 and all(self.board[r+i][c] == v for i in range(5)): return v
                if r <= BOARD_SIZE-5 and c <= BOARD_SIZE-5 and all(self.board[r+i][c+i] == v for i in range(5)): return v
                if r <= BOARD_SIZE-5 and c >= 4 and all(self.board[r+i][c-i] == v for i in range(5)): return v
        return EMPTY

    def process_move(self, r, c, color):
        # 棋步落地并触发哈希异或计算
        self.board[r][c] = color
        self.last_move = (r, c)
        self.history.append((r, c))
        # O(1) 更新哈希
        self.zobrist_hash ^= ZOBRIST_TABLE[r][c][color - 1]
        
        self.winner = self.check_win()
        if self.winner != EMPTY: 
            self.over = True
        else: 
            self.turn = 3 - color

    def run(self):
        # 主线程渲染与思考控制循环
        while True:
            self.draw_ui()
            pygame.display.flip() 

            if not self.over and self.turn == self.ai_color:
                if self.agent.check_surrender(self.board):
                    self.ai_surrendered = True
                    self.over = True
                    self.winner = self.human_color
                else:
                    # MTD(f)
                    # 借助于 Hash 缓存，在 Python 里将深度突破到了 6 步
                    # 意味着 AI 现在能看到：我-你-我-你-我-你 的未来。
                    move = self.agent.iterative_deepening(self.board, self.zobrist_hash, max_depth=6, color=self.ai_color)
                    
                    if move:
                        self.process_move(move[0], move[1], self.ai_color)
                    else:
                        self.over = True

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit()
                    
                if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                    if self.btn_black.collidepoint(event.pos): self.reset(BLACK)
                    elif self.btn_white.collidepoint(event.pos): self.reset(WHITE)
                    elif self.btn_undo.collidepoint(event.pos): self.undo()
                    
                    elif not self.over and self.turn == self.human_color:
                        x, y = event.pos
                        c, r = round((x-MARGIN)/CELL_SIZE), round((y-MARGIN)/CELL_SIZE)
                        if 0 <= r < BOARD_SIZE and 0 <= c < BOARD_SIZE and self.board[r][c] == EMPTY:
                            self.process_move(r, c, self.human_color)

if __name__ == "__main__":
    Game().run()