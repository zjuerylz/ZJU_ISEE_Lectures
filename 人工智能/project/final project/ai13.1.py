import pygame
import sys
import math

# 第一部分：基础配置与全局常量定义
BOARD_SIZE = 15          # 棋盘维度：15 x 15 标准五子棋棋盘
CELL_SIZE = 45           # 每个棋盘格子的像素大小
MARGIN = 40              # 棋盘边缘留白的像素大小
SIDEBAR_WIDTH = 250      # 右侧UI控制面板的宽度
# 自动计算窗口总宽高
WIDTH = BOARD_SIZE * CELL_SIZE + MARGIN * 2 + SIDEBAR_WIDTH
HEIGHT = BOARD_SIZE * CELL_SIZE + MARGIN * 2

# 颜色定义 (R, G, B)
COLOR_BOARD = (220, 179, 123)   # 棋盘底色（原木色）
COLOR_GRID = (60, 40, 20)       # 棋盘网格线颜色（深棕色）
COLOR_SIDEBAR = (45, 45, 48)    # 右侧控制面板底色（深灰黑）
COLOR_BTN = (70, 70, 75)        # 按钮默认颜色
COLOR_BTN_HOVER = (90, 90, 95)  # 按钮悬停时的颜色
COLOR_TEXT = (240, 240, 240)    # 文本颜色（亮白）

# 棋盘状态常量定义
EMPTY = 0   # 空位
BLACK = 1   # 黑棋（默认先手）
WHITE = 2   # 白棋（默认后手）

#评估长度为5的窗口中，己方连续棋子的价值。
SCORES = {
    5: 100000000,
    4: 1000000,
    3: 10000,
    2: 100,
    1: 10,
    0: 0
}
# 第二部分：Agent类
class Agent:
    def __init__(self, color):
        self.color = color              # AI执子的颜色
        self.opponent = 3 - color

    def check_surrender(self, board):
        surrender = 0
        
        # 内部扫描函数：检查传入的一维列表(直线)中是否存在绝对死局
        def scan_line(line):
            nonlocal surrender
            length = len(line)
            if length < 5: return
            
            # 1. 检查是否已经被对手连成了五子
            for i in range(length - 4):
                win5 = line[i:i+5]
                if win5.count(self.opponent) == 5: 
                    surrender += 1
            
            # 2. 检查对手是否形成了两头都没堵的“活四”
            for i in range(length - 5):
                win6 = line[i:i+6]
                if win6[0] == EMPTY and win6[5] == EMPTY and win6.count(self.opponent) == 4 and win6.count(self.color) == 0:
                    surrender += 1
                    
        # 提取棋盘的四个方向（横、竖、主对角线、副对角线）交由 scan_line 扫描
        for r in range(BOARD_SIZE): scan_line([board[r][c] for c in range(BOARD_SIZE)])
        for c in range(BOARD_SIZE): scan_line([board[r][c] for r in range(BOARD_SIZE)])
        for d in range(-BOARD_SIZE+1, BOARD_SIZE):
            scan_line([board[i][i-d] for i in range(BOARD_SIZE) if 0 <= i-d < BOARD_SIZE])
        for d in range(-BOARD_SIZE+1, BOARD_SIZE):
            scan_line([board[i][BOARD_SIZE-1-i-d] for i in range(BOARD_SIZE) if 0 <= BOARD_SIZE-1-i-d < BOARD_SIZE])
            
        return surrender >= 1 # 只要存在1个及以上的绝对死局，就判定为应当投降

    def evaluate_board(self, board):
        """
        叶子节点评估函数：
        在深搜到底部（Depth=0）时，对当前棋盘的整体优劣进行数学打分。
        逻辑：遍历全盘所有可能的长度为 5 的窗口，利用 SCORES 字典累加优势分。
        """
        score = 0
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                # 1. 横向窗口扫描
                if c <= BOARD_SIZE - 5:
                    my_c, op_c = 0, 0
                    for i in range(5):
                        p = board[r][c+i]
                        if p == self.color: my_c += 1
                        elif p == self.opponent: op_c += 1
                    # 只有当窗口内全是自己的子才加分，混杂敌子则视为无用废窗口
                    if my_c > 0 and op_c == 0: score += SCORES[my_c]
                    elif op_c > 0 and my_c == 0: score -= SCORES[op_c]
                
                # 2. 纵向窗口扫描
                if r <= BOARD_SIZE - 5:
                    my_c, op_c = 0, 0
                    for i in range(5):
                        p = board[r+i][c]
                        if p == self.color: my_c += 1
                        elif p == self.opponent: op_c += 1
                    if my_c > 0 and op_c == 0: score += SCORES[my_c]
                    elif op_c > 0 and my_c == 0: score -= SCORES[op_c]
                
                # 3. 主对角线窗口扫描
                if r <= BOARD_SIZE - 5 and c <= BOARD_SIZE - 5:
                    my_c, op_c = 0, 0
                    for i in range(5):
                        p = board[r+i][c+i]
                        if p == self.color: my_c += 1
                        elif p == self.opponent: op_c += 1
                    if my_c > 0 and op_c == 0: score += SCORES[my_c]
                    elif op_c > 0 and my_c == 0: score -= SCORES[op_c]
                
                # 4. 副对角线窗口扫描
                if r <= BOARD_SIZE - 5 and c >= 4:
                    my_c, op_c = 0, 0
                    for i in range(5):
                        p = board[r+i][c-i]
                        if p == self.color: my_c += 1
                        elif p == self.opponent: op_c += 1
                    if my_c > 0 and op_c == 0: score += SCORES[my_c]
                    elif op_c > 0 and my_c == 0: score -= SCORES[op_c]
                    
        return score

    def evaluate_point(self, board, r, c):
        """
        启发式点位评估函数：
        用于在不进行深搜的情况下，极速粗估某个空位的落子价值，用于后续的候选点排序。
        包含：自身落子收益、破坏敌方收益、防守惩罚、中心位置奖励。
        """
        if board[r][c] != EMPTY:
            return -math.inf # 已经有子的地方价值为负无穷
            
        val = 0
        directions = [(1,0), (0,1), (1,1), (1,-1)] # 四个考察方向
        
        # --- 模块1：收益评估 ---
        for dr, dc in directions:
            # 考察包含落子点 (r, c) 的所有长度为 5 的窗口
            for i in range(-4, 1):
                my_c, op_c = 0, 0
                valid = True
                
                # 统计当前窗口内双方棋子的数量
                for j in range(5):
                    nr, nc = r + (i+j)*dr, c + (i+j)*dc
                    if 0 <= nr < BOARD_SIZE and 0 <= nc < BOARD_SIZE:
                        p = board[nr][nc]
                        if p == self.color: my_c += 1
                        elif p == self.opponent: op_c += 1
                    else:
                        valid = False # 窗口超出了棋盘边界，无效
                        break
                        
                if valid:
                    # 进攻价值：如果这个窗口里没有敌方子，我下这里能让我连得更长
                    if op_c == 0: 
                        val += SCORES[my_c + 1]
                    # 防守价值：如果这个窗口里没有我的子，我下这里能破坏对手做局
                    if my_c == 0: 
                        # 特殊权重设计：防守优先，尤其是堵对手即将成四的三连子时
                        if op_c == 3: val += SCORES[4] * 1.5 
                        else: val += SCORES[op_c + 1]
                        
        # --- 模块2：预见性惩罚机制---
        # 如果我下在这里，会不会使得旁边的空位变成对手的绝杀点？
        predictive_penalty = 0
        board[r][c] = self.color
        
        for dr, dc in directions:
            for step in [-1, 1]:
                nr, nc = r + dr*step, c + dc*step
                # 如果旁边是个空位，我们要检查对手如果下在这里会有多大威力
                if 0 <= nr < BOARD_SIZE and 0 <= nc < BOARD_SIZE and board[nr][nc] == EMPTY:
                    op_count = 1
                    # 顺着方向数对手连着的子
                    for k in range(1, 5):
                        nnr, nnc = nr + k*dr, nc + k*dc
                        if 0 <= nnr < BOARD_SIZE and 0 <= nnc < BOARD_SIZE and board[nnr][nnc] == self.opponent:
                            op_count += 1
                        else: break
                    # 逆着方向数对手连着的子
                    for k in range(1, 5):
                        nnr, nnc = nr - k*dr, nc - k*dc
                        if 0 <= nnr < BOARD_SIZE and 0 <= nnc < BOARD_SIZE and board[nnr][nnc] == self.opponent:
                            op_count += 1
                        else: break
                        
                    # 如果对手下在这能连成4子或3子，说明我刚才下的 (r,c) 是极其危险的“自杀棋”
                    if op_count >= 4:
                        predictive_penalty += 500000 # 给予重罚
                    elif op_count == 3:
                        predictive_penalty += 20000  
                        
        board[r][c] = EMPTY # 评估完毕，恢复棋盘原状
        val -= predictive_penalty # 从总分中扣除风险惩罚

        # --- 模块3：中心偏好 ---
        # 减去该点离棋盘中心的曼哈顿距离。在开局局势不明朗时，驱使AI往中心下棋
        val -= abs(r - BOARD_SIZE//2) + abs(c - BOARD_SIZE//2)
        
        return val

    def get_candidates(self, board, depth):
        """
        启发式候选节点生成器（包含 Beam Search 动态分支收束）：
        为了优化Alpha-Beta树的搜索性能，我们不能遍历全盘所有的空位，
        而是挑选出最有可能的高价值点位。
        """
        candidates = []
        has_piece = False
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                if board[r][c] != EMPTY:
                    has_piece = True
                    continue
                
                # 半径缩小剪枝：只搜索距离已有棋子周围 3 格范围内的空位
                # 远离战区的孤立空位不予考虑，极大节省算力
                near = False
                for i in range(max(0, r-3), min(BOARD_SIZE, r+4)):
                    for j in range(max(0, c-3), min(BOARD_SIZE, c+4)):
                        if board[i][j] != EMPTY:
                            near = True; break
                    if near: break
                    
                # 如果这个点在战区内，对它进行粗估打分并加入候选列表
                if near:
                    val = self.evaluate_point(board, r, c)
                    candidates.append((val, r, c))
                    
        # 特殊情况处理：如果是开局第一手，直接下在天元（正中心）
        if not has_piece: 
            return [(BOARD_SIZE//2, BOARD_SIZE//2)]
            
        # 根据刚才粗估的分数，从大到小降序排序（Move Ordering）
        # 让最高价值的点最先被搜索
        candidates.sort(key=lambda x: x[0], reverse=True)
        
        # 动态分支收束 (Dynamic Beam Pruning)
        # 根据当前还剩下的深搜步数，决定这一层只看几个点。
        # 浅层看广，深层看精。这是 Depth=4 依然流畅的核心秘诀。
        if depth >= 4: limit = 12
        elif depth == 3: limit = 8
        elif depth == 2: limit = 5
        else: limit = 3
        
        # 绝杀特权收束：如果首选点分数极高（大于千万），说明已经面临绝杀或被绝杀，
        # 此时放弃广度搜索，强制收束到前两个点位
        if candidates and candidates[0][0] >= 1e7:
            limit = 2
            
        # 截取 Top N 候选点并返回坐标
        return [(r, c) for _, r, c in candidates[:limit]] 

    def alphabeta(self, board, depth, alpha, beta, is_maximizing):
        """
        极大极小值算法与 Alpha-Beta 剪枝核心逻辑：
        让 AI 在脑海中预演未来的交替落子。
        is_maximizing: True 代表这是AI的回合（追求最高分），False 代表这是玩家的回合（追求最低分）。
        """
        score = self.evaluate_board(board)
        # 递归终止条件：搜索深度耗尽，或者棋盘上已经决出胜负
        if depth == 0 or abs(score) >= 100000000:
            return score, None

        # 获取当前局面的高质量候选点
        candidates = self.get_candidates(board, depth)
        if not candidates: return score, None
        best_move = candidates[0]

        if is_maximizing:
            max_val = -math.inf
            for r, c in candidates:
                # 试探性落子
                board[r][c] = self.color
                # 递归进入下一层（玩家的回合），深度-1
                val, _ = self.alphabeta(board, depth-1, alpha, beta, False)
                # 回溯：撤销试探性落子，恢复棋盘
                board[r][c] = EMPTY
                
                # 如果这个分支结果比当前已知最好结果还要好，更新最优解
                if val > max_val:
                    max_val, best_move = val, (r, c)
                
                # 更新当前Max层保底的下界 Alpha
                alpha = max(alpha, val)
                
                # Beta 剪枝：如果当前发现的下界 Alpha 已经超过了上层 Min 层能容忍的上界 Beta
                # 说明上层的玩家绝对不会允许走到这个分支，直接砍掉后续所有无效搜索
                if beta <= alpha: break
            return max_val, best_move
            
        else:
            min_val = math.inf
            for r, c in candidates:
                board[r][c] = self.opponent
                val, _ = self.alphabeta(board, depth-1, alpha, beta, True)
                board[r][c] = EMPTY
                
                if val < min_val:
                    min_val, best_move = val, (r, c)
                    
                # 更新当前Min层容忍的上界 Beta
                beta = min(beta, val)
                # Alpha 剪枝
                if beta <= alpha: break
            return min_val, best_move


# 第三部分：前端界面与控制逻辑 (Game类)
class Game:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((WIDTH, HEIGHT))
        pygame.display.set_caption("AI Gomoku - Smart Undo & True Surrender")
        # 初始化界面字体
        self.font = pygame.font.Font(None, 24) 
        self.big_font = pygame.font.Font(None, 28)
        self.huge_font = pygame.font.Font(None, 80)
        
        self.reset(BLACK) # 默认玩家执黑先手启动

    def reset(self, human_color):
        """重置整局游戏，清理棋盘和历史记录"""
        self.board = [[EMPTY for _ in range(BOARD_SIZE)] for _ in range(BOARD_SIZE)]
        self.human_color = human_color
        self.ai_color = 3 - human_color
        self.turn = BLACK # 黑棋永远是第一手
        self.over = False
        self.ai_surrendered = False
        self.winner = EMPTY
        self.agent = Agent(self.ai_color)
        self.history = [] # 历史记录栈，用于悔棋
        self.last_move = None

    def undo(self):
        """
        智能悔棋逻辑：
        动态识别历史单双数，确保无论AI是否中途认输，都能精准归还人类回合。
        """
        if not self.history:
            return # 空棋盘不能悔棋
            
        # 约束条件：必须轮到人类回合，或者游戏已经结束时，才允许玩家点击悔棋
        if not (self.turn == self.human_color or self.over):
            return
            
        # 先无条件在历史记录栈里弹走最后一步
        self.history.pop()
        
        # 继续弹走记录，直到下一个下棋的人正好是人类为止
        while self.history:
            # 推断如果历史长度为偶数，下一手应为黑；奇数则为白
            next_turn = WHITE if len(self.history) % 2 != 0 else BLACK
            if next_turn == self.human_color:
                break
            self.history.pop()
            
        # 根据弹空后的记录栈，重新搭建棋盘
        self.rebuild_board()

    def rebuild_board(self):
        """修复了回合错位 Bug 的棋盘重建逻辑"""
        self.board = [[EMPTY for _ in range(BOARD_SIZE)] for _ in range(BOARD_SIZE)]
        current_turn = BLACK
        self.last_move = None
        self.ai_surrendered = False # 解除认输状态
        
        # 遍历历史记录，依次在空棋盘上落子复原
        for r, c in self.history:
            self.board[r][c] = current_turn
            self.last_move = (r, c)
            current_turn = 3 - current_turn
            
        # 把下一回合的归属权下发，避免人类出现“一回合下多子”
        self.turn = current_turn 
        self.over = False
        self.winner = EMPTY

    def draw_stone(self, r, c, color):
        """使用 Pygame 绘制具有 3D 视觉高光与阴影的高保真棋子"""
        x, y = MARGIN + c * CELL_SIZE, MARGIN + r * CELL_SIZE
        # 1. 绘制底层偏移的半透明阴影，制造悬浮感
        pygame.draw.circle(self.screen, (40, 40, 40), (x+2, y+2), CELL_SIZE//2 - 2)
        # 2. 绘制棋子基础底色
        main_color = (20, 20, 20) if color == BLACK else (240, 240, 240)
        pygame.draw.circle(self.screen, main_color, (x, y), CELL_SIZE//2 - 2)
        # 3. 绘制左上角的反光高光点，模拟光源照射增强 3D 立体感
        high_color = (80, 80, 80) if color == BLACK else (255, 255, 255)
        pygame.draw.circle(self.screen, high_color, (x-4, y-4), CELL_SIZE//6)

    def draw_ui(self):
        """渲染每一帧的UI界面"""
        # 绘制背景板
        self.screen.fill(COLOR_BOARD)
        # 绘制 15x15 的网格交叉线
        for i in range(BOARD_SIZE):
            pygame.draw.line(self.screen, COLOR_GRID, (MARGIN, MARGIN+i*CELL_SIZE), (MARGIN+(BOARD_SIZE-1)*CELL_SIZE, MARGIN+i*CELL_SIZE), 1)
            pygame.draw.line(self.screen, COLOR_GRID, (MARGIN+i*CELL_SIZE, MARGIN), (MARGIN+i*CELL_SIZE, MARGIN+(BOARD_SIZE-1)*CELL_SIZE), 1)
        
        # 绘制棋盘上的所有棋子
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                if self.board[r][c] != EMPTY:
                    self.draw_stone(r, c, self.board[r][c])
        
        # 用红框高亮显示最新落下的一步棋，方便玩家定位
        if self.last_move:
            r, c = self.last_move
            pygame.draw.rect(self.screen, (255, 0, 0), (MARGIN+c*CELL_SIZE-5, MARGIN+r*CELL_SIZE-5, 10, 10), 2)

        # 渲染AI投降
        if self.ai_surrendered:
            overlay = pygame.Surface((WIDTH - SIDEBAR_WIDTH, HEIGHT))
            overlay.set_alpha(150) # 设置半透明度
            overlay.fill((50, 50, 50))
            self.screen.blit(overlay, (0, 0))
            
            win_txt = self.huge_font.render("YOU WIN", True, (255, 215, 0)) # 金色大字
            win_rect = win_txt.get_rect(center=((WIDTH - SIDEBAR_WIDTH)//2, HEIGHT//2 - 20))
            self.screen.blit(win_txt, win_rect)
            
            sub_txt = self.big_font.render("AI Surrendered!", True, (255, 255, 255))
            sub_rect = sub_txt.get_rect(center=((WIDTH - SIDEBAR_WIDTH)//2, HEIGHT//2 + 40))
            self.screen.blit(sub_txt, sub_rect)

        # 绘制右侧控制面板底色
        sidebar_rect = pygame.Rect(WIDTH - SIDEBAR_WIDTH, 0, SIDEBAR_WIDTH, HEIGHT)
        pygame.draw.rect(self.screen, COLOR_SIDEBAR, sidebar_rect)
        
        # 右上角的系统状态提示文本
        if self.ai_surrendered:
            status = "AI Gave Up!"
        elif self.over:
            status = "Game Over"
        else:
            status = "AI Thinking..." if self.turn == self.ai_color else "Your Turn"
            
        txt = self.big_font.render(status, True, COLOR_TEXT)
        self.screen.blit(txt, (WIDTH - SIDEBAR_WIDTH + 20, 50))

        # 游戏结束时公布获胜方
        if self.over and not self.ai_surrendered:
            win_txt = "Winner: " + ("Black" if self.winner == BLACK else "White")
            self.screen.blit(self.big_font.render(win_txt, True, (255, 200, 0)), (WIDTH - SIDEBAR_WIDTH + 20, 90))

        # 绘制侧边栏交互按钮，并保存它们的碰撞检测矩形区 (Rect)
        self.btn_black = self.draw_button("Restart (Play Black)", 200)
        self.btn_white = self.draw_button("Restart (Play White)", 270)
        self.btn_undo  = self.draw_button("Undo Move", 340)

    def draw_button(self, text, y_pos):
        """按钮绘制组件：带鼠标悬停变色效果"""
        mouse_pos = pygame.mouse.get_pos()
        rect = pygame.Rect(WIDTH - SIDEBAR_WIDTH + 20, y_pos, 210, 45)
        # 悬停检测
        color = COLOR_BTN_HOVER if rect.collidepoint(mouse_pos) else COLOR_BTN
        pygame.draw.rect(self.screen, color, rect, border_radius=8)
        
        # 渲染按钮文字并居中对齐
        txt = self.font.render(text, True, COLOR_TEXT)
        self.screen.blit(txt, (rect.centerx - txt.get_width()//2, rect.centery - txt.get_height()//2))
        return rect

    def check_win(self):
        """游戏结束判定：检查棋盘上是否有玩家已经连成五子"""
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                v = self.board[r][c]
                if v == EMPTY: continue
                # 利用列表推导式验证横、竖、斜方向上是否出现连续 5 个相同颜色的棋子
                if c <= BOARD_SIZE-5 and all(self.board[r][c+i] == v for i in range(5)): return v
                if r <= BOARD_SIZE-5 and all(self.board[r+i][c] == v for i in range(5)): return v
                if r <= BOARD_SIZE-5 and c <= BOARD_SIZE-5 and all(self.board[r+i][c+i] == v for i in range(5)): return v
                if r <= BOARD_SIZE-5 and c >= 4 and all(self.board[r+i][c-i] == v for i in range(5)): return v
        return EMPTY # 如果没找到，返回 EMPTY(0) 表示还没人赢

    def process_move(self, r, c, color):
        """处理一次有效的落子动作：写入棋盘、添加历史记录、判定胜负、移交回合权"""
        self.board[r][c] = color
        self.last_move = (r, c)
        self.history.append((r, c))
        
        self.winner = self.check_win()
        if self.winner != EMPTY: 
            self.over = True
        else: 
            self.turn = 3 - color # 黑白棋权交替

    def run(self):
        """Pygame 游戏主循环引擎"""
        while True:
            # 1. 刷新界面 UI
            self.draw_ui()
            pygame.display.flip() 

            # 2. 如果游戏未结束且当前是 AI 的回合，触发思考机制
            if not self.over and self.turn == self.ai_color:
                # 首先进行独立的投降判定：如果人类已造出真实无解活四，则有风度地直接认输
                if self.agent.check_surrender(self.board):
                    self.ai_surrendered = True
                    self.over = True
                    self.winner = self.human_color
                else:
                    # 如果棋盘上没有绝对的死局，也必须选出“最优防守点”来继续游戏
                    score, move = self.agent.alphabeta(self.board, 4, -math.inf, math.inf, True)
                    if move:
                        self.process_move(move[0], move[1], self.ai_color)
                    else:
                        # 极端罕见情况：如果没地方可下，游戏也强制结束
                        self.over = True

            # 3. 拦截并处理用户的鼠标/键盘事件
            for event in pygame.event.get():
                if event.type == pygame.QUIT: # 点击右上角关闭窗口
                    pygame.quit()
                    sys.exit()
                    
                if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1: # 鼠标左键点击
                    # 检测是否点击到了侧边栏的控制按钮
                    if self.btn_black.collidepoint(event.pos): self.reset(BLACK)
                    elif self.btn_white.collidepoint(event.pos): self.reset(WHITE)
                    elif self.btn_undo.collidepoint(event.pos): self.undo()
                    
                    # 检测是否点击到了棋盘格落子
                    elif not self.over and self.turn == self.human_color:
                        x, y = event.pos
                        # 将鼠标的物理像素坐标四舍五入折算成棋盘上的行列索引 (r, c)
                        c, r = round((x-MARGIN)/CELL_SIZE), round((y-MARGIN)/CELL_SIZE)
                        # 如果点击在有效棋盘内且该位置是空的，则执行玩家落子
                        if 0 <= r < BOARD_SIZE and 0 <= c < BOARD_SIZE and self.board[r][c] == EMPTY:
                            self.process_move(r, c, self.human_color)

# 脚本入口执行
if __name__ == "__main__":
    Game().run()