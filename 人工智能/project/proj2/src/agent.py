from utils import Board, Player, Record
from utils import opposite, make_state
from typing import Tuple
import math
import random

class Agent:
    def __init__(self, color: Player, algorithm='minimax', max_iter=100):
        """
        Args:
            color (Player): AI 选择的颜色
            algorithm ('minimax' | 'alphabeta' | 'ucb1'): Agent 采取的算法. default: 'minimax'
            max_iter: MCTS 模拟次数. default: 100
        """
        self.color = color
        self.algorithm = algorithm

        # variables for MCTS
        self.max_iter = max_iter
        self.history = {}

    def action(self, board: Board) -> Tuple[int, int]:
        """
        Args:
            board: 棋盘
        Returns:
            action: (x, y). AI决策的落子位置
        """
        best_score = float('-inf')
        best_move = None

        if self.algorithm == 'ucb1':
            # 记录根结点
            self.history = {
                make_state(board, self.color) : Record(board.actions(self.color))
            }
            # MCTS
            for _ in range(self.max_iter):
                self.MCTS(board, self.color)
        for action in board.actions(self.color):
            # 枚举每个可能的落子方式，并获取最佳的落子方式
            if self.algorithm == 'minimax':
                score = self.minimax(board.result(action, self.color), opposite(self.color))
            elif self.algorithm == 'alphabeta':
                score = self.alphabeta(board.result(action, self.color), opposite(self.color))
            elif self.algorithm == 'ucb1':
                # 根据历史信息计算行动的平均效用
                score = self.ucb1(board, self.color, action, real=True)
            else:
                raise NotImplementedError
            if score >= best_score:
                best_move = action
                best_score = score
                if self.algorithm == 'alphabeta' and best_score == 1:
                    # 已经找到必胜的落子方式，无需继续搜索
                    break

        if best_move is None:
            raise ValueError
        return best_move

    def minimax(self, board: Board, player: Player) -> int:
        """
        Minmax 算法
        Args:
            board: 棋盘
            player: 棋盘的落子方
        Returns:
            score: 算法决策计算得到效用
        """
        if board.terminated():
            # 到达终止状态，计算对应的效用函数
            return board.utility(self.color)
        # 计算每个落子方式对应的回报
        scores = [self.minimax(board.result(action, player), opposite(player))
                  for action in board.actions(player)]

        if player == self.color:
            # 当前是自己的回合，选择能使自己效用最大的行动
            return max(scores)
        else:
            # 当前是对手的回合，对手会选择使我效用最小的行动
            return min(scores)
        """
        TODO 5:
            请编写合理的代码段。
            这部分代码将实现Minmax算法中根据当前的行动方player选择回报的最大值还是最小值。
        Note:
            根据当前的行动方，决定取这些回报中的最大值还是最小值作为当前结点的效用。
        """


    def alphabeta(self, board: Board, player: Player, alpha: int = -1, beta: int = 1) -> int:
        """
        Alpha-beta 剪枝
        Args:
            board: 棋盘
            player: 当前状态的行动方
            alpha, beta: alpha-beta 区间
        Returns:
            score: 算法决策计算得到效用
        """
        if board.terminated():
            # 终止状态
            return board.utility(self.color)
        
        # 初始化回报v
        if player == self.color:
            v = -1
        else:
            v = 1
        # 枚举落子方式
        for action in board.actions(player):
            if player == self.color:
                v = max(v, self.alphabeta(board.result(action, player), opposite(player), alpha, beta))
                if v >= beta:
                    # 区间为空，剪枝
                    break
                # 更新区间
                alpha = max(alpha, v)
            else:
                # 对手回合，对手会最小化我的效用
                v = min(v, self.alphabeta(board.result(action, player), opposite(player), alpha, beta))
                if v <= alpha:
                    # 当前值已经小于等于 alpha，父节点（max节点）不会选择这个分支，可以剪枝
                    break
                # 更新 beta 值，缩小搜索区间
                beta = min(beta, v)
                """
                TODO 6:
                    请编写合理的代码段。
                    这部分代码将实现Min的情况下的搜索和区间更新。
                Note:
                    请参考Max的情况的实现。
                """

        return v

    def ucb1(self, board: Board, player: Player, action, real=False) -> float:
        """
        UCB1 score
        根据board和player，从历史信息中计算ucb1
        Args:
            board: 棋盘
            player: 落子方
            real: 若为True则直接计算效用均值，若为False则给出UCB1
        Returns:
            score: UCB1 score
        """
        state = make_state(board, player)
        factor = 1 if player == self.color else -1
        if real:
            factor = 0

        if state in self.history:
            record : Record = self.history[state]
            if action in record.actions:
                return record.average(action) + factor * (2 * math.log(record.n) / record.times(action)) ** 0.5
        return float('inf') * factor

    def MCTS(self, board: Board, player: Player, rollout=False) -> int:
        """
        Monte-Carlo Tree Search
        Args:
            board: 当前结点的局面
            player: 当前结点的落子方
            rollout: 快速模拟标记. 若为True则为快速模拟阶段
        Returns:
            score: 返回效用，用于更新结点信息
        """
        if board.terminated():
            return board.utility(self.color)

        if not rollout:
            record = self.history[make_state(board, player)]
            # 未完全展开的结点, Expand
            if len(record.rest_actions) > 0:
                # Expand
                # 选择叶子结点
                action = random.choice(record.rest_actions)
                record.rest_actions.remove(action)
                new_board = board.result(action, player)
                # 注意：在本规则下，博弈树并不严格保证不同分支之间的不相交，可能存在不同的结点指向同一个状态的情况
                if make_state(new_board, opposite(player)) not in self.history:
                    self.history[make_state(new_board, opposite(player))] = Record(new_board.actions(opposite(player)))
                # Rollout
                score = self.MCTS(new_board, opposite(player), rollout=True)
            # 结点完全展开, Select
            else:
                # 根据ucb1选择最佳策略
                best_action = None
                best_ucb = float('-inf')
                for a in record.actions.keys():
                    ucb = self.ucb1(board, player, a)
                    if ucb > best_ucb:
                        best_ucb = ucb
                        best_action = a
                action = best_action
                new_board = board.result(action, player)
                score = self.MCTS(new_board, opposite(player), rollout=True)
                """
                TODO 10:
                    请编写合理的代码段。
                    这部分代码将实现MCTS的Select部分
                Note:
                    可以通过调用`self.ucb1(board, player, action)`来计算`action`对应的UCB1值
                    注意根据当前的行动方player，选择UCB1值最大的行动还是最小的行动
                """

            # Backpropagation
            # 回溯更新历史信息
            state = make_state(board, player)
            self.history[state].update(score, action)
        else:
            # Rollout
            action = random.choice(board.actions(player))
            new_result = board.result(action, player)
            score = self.MCTS(new_result, opposite(player), rollout=True)
        return score
