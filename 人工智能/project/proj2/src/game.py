from typing import List, Tuple
from utils import Board, Color, Player
from utils import WHITE, BLACK, EMPTY, opposite
from agent import Agent
import copy
import time

class Game:
    def __init__(self, human : Player = BLACK, ai_method : str = 'minimax', max_iter : int = 100, board_size: int = 4) -> None:
        self.human: Player = human
        self.turn: Player = BLACK
        self.step = 0
        self._board = Board(board_size=board_size)
        self._over = False
        self.agent = Agent(opposite(self.human), algorithm=ai_method, max_iter=max_iter)

    @property
    def board(self) -> List[List[Color]]:
        return copy.deepcopy(self._board.board)

    @property
    def over(self) -> bool:
        return self._board.terminated()

    @property
    def winner(self) -> Color:
        return self._board.winner()

    def _change_turn(self) -> None:
        self.turn = opposite(self.turn)

    def place(self, pos: Tuple[int, int]) -> None:
        if pos in self._board.actions(self.human):
            self._board = self._board.result(pos, self.turn)
            self._change_turn()

    def ai_step(self) -> None:
        time.sleep(0.2)
        if self._board.actions(self.turn)[0] == None:
            # ai pass
            self._change_turn()
        else:
            pos = self.agent.action(copy.deepcopy(self._board))
            self._board = self._board.result(pos, self.turn)
            # check if not human pass
            if self.over or self._board.actions(opposite(self.turn))[0] is not None:
                self._change_turn()
