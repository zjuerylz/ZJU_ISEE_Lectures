from typing import Literal, Union, Tuple, List
import copy

WHITE = 0
BLACK = 1
EMPTY = None

Color = int | None
Player = Color


def opposite(player: Player):
    """
    Args:
        player
    Return:
        opposite_player: 根据输入方返回其对手
    """
    if player != None:
        return 1 - player
    else:
        raise ValueError


class Board:
    def __init__(self, board=None, board_size=4) -> None:
        self.board_size = board_size
        if board is None:
            self.board =  [[EMPTY for _ in range(board_size)] for _ in range(board_size)]
            self.board[board_size // 2 - 1][board_size // 2 - 1] = WHITE
            self.board[board_size // 2 - 1][board_size // 2] = BLACK
            self.board[board_size // 2][board_size // 2 - 1] = BLACK
            self.board[board_size // 2][board_size // 2] = WHITE
        else:
            self.board = copy.deepcopy(board)

    def terminated(self) -> bool:
        """
        判断棋局是否结束
        """
        # 双方都无法下了，游戏结束
        return self.actions(WHITE)[0] == None and self.actions(BLACK)[0] == None

    def actions(self, player: Player) -> List[Tuple[int, int]]:
        """
        给定落子方，返回能够落子的位置
        Args:
            player: 落子方
        Returns:
            actions: 可落子的位置，若轮空则为`[None]`
        """
        ret = []
        for i in range(self.board_size):
            for j in range(self.board_size):
                if self.board[i][j] == EMPTY:
                    flag = False # 用于标记落子在(i, j)处是否能够引起翻转
                    # 检查落子在此处时，4个方向是否能够引起翻转
                    ## 上
                    if i > 0 and self.board[i - 1][j] == opposite(player):
                        for k in reversed(range(0, i - 1)):
                            if self.board[k][j] == player:
                                ret.append((i, j))
                                flag = True
                                break
                            elif self.board[k][j] == EMPTY:
                                break
                    ## 下
                    if not flag and i < self.board_size - 1 and self.board[i + 1][j] == opposite(player):
                        for k in range(i + 2, self.board_size):
                            if self.board[k][j] == player:
                                ret.append((i, j))
                                flag = True
                                break
                            elif self.board[k][j] == EMPTY:
                                break
                    ## 左
                    if not flag and j > 0 and self.board[i][j - 1] == opposite(player):
                        for k in reversed(range(0, j - 1)):
                            if self.board[i][k] == player:
                                ret.append((i, j))
                                flag = True
                                break
                            elif self.board[i][k] == EMPTY:
                                break
                    """
                    TODO 1:
                        请编写合理的代码段。
                        这部分代码将实现水平方向向左的落子检查。
                    Note:
                        请参考竖直方向向上的检查的实现。
                    """

                    ## 右
                    if not flag and j < self.board_size - 1 and self.board[i][j + 1] == opposite(player):
                        for k in range(j + 2, self.board_size):
                            if self.board[i][k] == player:
                                ret.append((i, j))
                                flag = True
                                break
                            elif self.board[i][k] == EMPTY:
                                break
                    """
                    TODO 2:
                        请编写合理的代码段。
                        这部分代码将实现水平方向向右的落子检查。
                    Note:
                        请参考竖直方向向下的检查的实现。
                    """

        if len(ret) == 0:
            # Pass
            ret.append(None)
        return ret

    def result(self, pos: Union[None, Tuple[int, int]], player: Player):
        """
        根据落子方和其采取的行动产生对应的新局面
        Args:
            pos: 落子方采取的行动
            player: 落子方
        Returns:
            board: 行动后的新局面
        """
        if pos is None:
            # 轮空
            return Board(copy.deepcopy(self.board))

        x, y = pos
        board = copy.deepcopy(self.board)
        # 对4个方向做翻转
        ## 上
        if x > 0 and board[x - 1][y] == opposite(player):
            for i in reversed(range(0, x - 1)):
                if board[i][y] == player:
                    for j in range(i + 1, x):
                        board[j][y] = player
                    break
                elif board[i][y] == EMPTY:
                    break
        ## 下
        if x < self.board_size - 1 and board[x + 1][y] == opposite(player):
            for i in range(x + 2, self.board_size):
                if board[i][y] == player:
                    for j in range(x + 1, i):
                        board[j][y] = player
                    break
                elif board[i][y] == EMPTY:
                    break
        ## 左
        if y > 0 and board[x][y - 1] == opposite(player):
            for k in reversed(range(0, y - 1)):
                if board[x][k] == player:
                    for l in range(k + 1, y):
                        board[x][l] = player
                    break
                elif board[x][k] == EMPTY:
                    break
        """
        TODO 3:
            请编写合理的代码段。
            这部分代码将实现水平方向向左的翻转。
        Note:
            请参考竖直方向向上的翻转的实现。
        """
        ## 右
        if y < self.board_size - 1 and board[x][y + 1] == opposite(player):
            for k in range(y + 2, self.board_size):
                if board[x][k] == player:
                    for l in range(y + 1, k):
                        board[x][l] = player
                    break
                elif board[x][k] == EMPTY:
                    break
        """
        TODO 4:
            请编写合理的代码段。
            这部分代码将实现水平方向向右的翻转。
        Note:
            请参考竖直方向向下的翻转的实现。
        """

        board[x][y] = player
        return Board(board=board, board_size=self.board_size)

    def utility(self, player: Player) -> Literal[-1, 0, 1]:
        """
        效用函数
        Args:
            player: 选择的棋手
        Returns:
            utility: 表示对棋手player而言当前局面的胜负产生的效用值
                - 1: 当前局面player获胜
                - 0: 当前局面为平局
                - -1: 当前局面player告负
        """
        winner = self.winner()
        if winner == player:
            return 1
        if winner == opposite(player):
            return -1
        return 0

    def winner(self) -> Player:
        """
        Returns:
            winner:
                - WHITE 表示白方获胜
                - BLACK 表示黑方获胜
                - EMPTY 表示无人获胜，即平局
        """
        nWhite = 0
        nBlack = 0
        for i in range(self.board_size):
            for j in range(self.board_size):
                if self.board[i][j] == WHITE:
                    nWhite += 1
                elif self.board[i][j] == BLACK:
                    nBlack += 1
        if nWhite > nBlack:
            return WHITE
        elif nWhite < nBlack:
            return BLACK
        else:
            return EMPTY

    def compact(self) -> str:
        """
        将棋盘转化成字符串的形式，便于在dict中作为key使用
        Returns:
            repr: 一个表示当前棋盘的字符串
        """
        repr = ''
        for i in range(self.board_size):
            for j in range(self.board_size):
                if self.board[i][j] == EMPTY:
                    repr += 'E'
                elif self.board[i][j] == WHITE:
                    repr += 'W'
                else:
                    repr += 'B'
        return repr


def make_state(board: Board, player: Player):
    """
    根据棋盘和落子方构造一个不可变值，用于表示搜索树的结点状态
    Args:
        board: 棋盘
        player: 落子方
    Returns:
        state: 一个由board和player拼接的表示
    """
    s = (board.compact(), player)
    return s


class Record:
    """
    用于记录结点历史信息的结构体
    Args:
        actions: 该结点的可选行动方式
    """
    def __init__(self, actions) -> None:
        self.n = 0
        self.actions = {}
        self.rest_actions = actions

    def update(self, v: int, action) -> None:
        """
        更新结点信息
        Args:
            v: 反向传播得到的效用值
            action: 本次模拟中结点选择的行动方式
        """
        # 结点总访问次数加1
        self.n += 1
        # 如果该行动尚未记录，初始化为 [总效用, 次数]
        if action not in self.actions:
            self.actions[action] = [0, 0]
        # 更新该行动的累计效用和次数
            self.actions[action][0] += v
            self.actions[action][1] += 1
        """
        TODO 7:
            请编写合理的代码段。
            这部分代码将实现反向传播时结点信息的更新。
        Note:
            只需修改`self.actions`中的内容，注意判断`action`是否出现在`self.actions`中。
        """



    def average(self, action) -> float:
        """
        历史行动的效用的均值
        Args:
            action: 被询问的行动
        Returns:
            x_bar: 历史中结点选择了行动action得到的效用的均值
        """
        if action in self.actions:
            total, count = self.actions[action]
            return total / count
        else:
            return 0
        """
        TODO 8:
            请编写合理的代码段。
            这部分代码将返回当前结点的历史行动的效用均值。
        Note:
            可从`self.actions`中查询相关信息，注意判断`action`是否出现在`self.actions`中。
        """



    def times(self, action) -> int:
        """
        历史中行动的次数
        Args:
            action: 被询问的行动
        Returns:
            n: 历史中结点选择行动action的次数
        """
        if action in self.actions:
            # 返回该行动的访问次数（存储在列表的第二个元素）
            return self.actions[action][1]
        else:
            return 0
        """
        TODO 9:
            请编写合理的代码段。
            这部分代码将返回当前结点的历史行动的行动次数。
        Note:
            可从`self.actions`中查询相应信息，注意判断`action`是否出现在`self.actions`中。
        """
