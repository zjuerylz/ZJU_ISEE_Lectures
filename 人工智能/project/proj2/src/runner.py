# runner.py
import pygame
import sys
import time
import argparse
from game import Game
from utils import Color
from utils import WHITE, BLACK, EMPTY


parser = argparse.ArgumentParser(description="Reversi")
parser.add_argument("--ai_method", type=str, choices=["minimax", "alphabeta", "ucb1"], default="minimax")
parser.add_argument("--max_iter", type=int, default=100)
parser.add_argument("--board_size", type=int, default=4, choices=[4, 6, 8], help="The size of the board. default: 4")
args = parser.parse_args()

pygame.init()
size = WIDTH, HEIGHT = 960, 800

# Colors
black = (0, 0, 0)
white = (255, 255, 255)
grey = (128, 128, 128)
pygame.display.set_caption("reversi")
screen = pygame.display.set_mode(size)

mediumFont = pygame.font.Font("OpenSans-Regular.ttf", 28)
largeFont = pygame.font.Font("OpenSans-Regular.ttf", 40)
moveFont = pygame.font.Font("OpenSans-Regular.ttf", 60)

user = None
game = None

while True:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            sys.exit()
    screen.fill(grey)

    if user is None:
        # Draw title
        title = largeFont.render("Play Reversi", True, white)
        titleRect = title.get_rect()
        titleRect.center = (WIDTH // 2, 50)
        screen.blit(title, titleRect)

        # Draw buttons
        playWButton = pygame.Rect(WIDTH // 8, HEIGHT // 2, WIDTH // 3, 50)
        playW = mediumFont.render("Play as White", True, black)
        playWRect = playW.get_rect()
        playWRect.center = playWButton.center
        pygame.draw.rect(screen, white, playWButton)
        screen.blit(playW, playWRect)

        playBButton = pygame.Rect(WIDTH // 8 * 5, HEIGHT // 2, WIDTH // 3, 50)
        playB = mediumFont.render("Play as Black", True, black)
        playBRect = playB.get_rect()
        playBRect.center = playBButton.center
        pygame.draw.rect(screen, white, playBButton)
        screen.blit(playB, playBRect)

        # Check if button is clicked
        click, _, _ = pygame.mouse.get_pressed()
        if click == 1:
            mouse = pygame.mouse.get_pos()
            if playWButton.collidepoint(mouse):
                time.sleep(0.2)
                user = WHITE
                game = Game(user, args.ai_method, args.max_iter, board_size=args.board_size)
            elif playBButton.collidepoint(mouse):
                time.sleep(0.2)
                user = BLACK
                game = Game(user, args.ai_method, args.max_iter, board_size=args.board_size)
    else:
        assert isinstance(game, Game)
        # Draw game board
        board = game.board
        tile_size = int(min(WIDTH, HEIGHT) * 0.75 // args.board_size)
        tile_origin = (WIDTH // 2 - args.board_size // 2 * tile_size - 100,
                       HEIGHT // 2 - args.board_size // 2 * tile_size + 40)

        for i in range(args.board_size):
            for j in range(args.board_size):
                rect = pygame.Rect(
                    tile_origin[0] + j * tile_size,
                    tile_origin[1] + i * tile_size,
                    tile_size, tile_size)
                pygame.draw.rect(screen, white, rect, 3)
                if board[i][j] != EMPTY:
                    if board[i][j] == WHITE:
                        cir_col = white
                    else:
                        cir_col = black
                    pygame.draw.circle(screen, cir_col, rect.center, 35, 0)

        if game.over:
            winner = game.winner
            if winner == EMPTY:
                title = f"Game Over: Tie"
            elif winner == BLACK:
                title = "Game Over: BLACK wins"
            else:
                title = "Game Over: WHITE wins"
        elif user == game.turn:
            user_color = {BLACK: "BLACK", WHITE: "WHITE"}[user]
            title = f"Play as {user_color}"
        else:
            title = f"Computer thinking..."

        title = largeFont.render(title, True, black)
        titleRect = title.get_rect()
        titleRect.center = (WIDTH // 2, 30)
        screen.blit(title, titleRect)
        if game.over:
            againButton = pygame.Rect(
                WIDTH - 200, HEIGHT - 100, WIDTH / 4, 50)
            again = mediumFont.render("Play Again", True, black)
            againRect = again.get_rect()
            againRect.center = againButton.center
            pygame.draw.rect(screen, white, againButton)
            screen.blit(again, againRect)

            click, _, _ = pygame.mouse.get_pressed()
            if click == 1:
                mouse = pygame.mouse.get_pos()
                if againButton.collidepoint(mouse):
                    time.sleep(0.2)
                    user = None
                    game = None
            pygame.display.flip()         
        elif user != game.turn:
            # AI's turn
            pygame.display.flip()
            game.ai_step()
        else:
            pygame.display.flip()
            click, _, _ = pygame.mouse.get_pressed()
            if click == 1 and user == game.turn:
                px, py = pygame.mouse.get_pos()
                px = px - tile_origin[0]
                py = py - tile_origin[1]
                if 0 <= px < args.board_size * tile_size \
                    and 0 <= py < args.board_size * tile_size:
                    game.place((py // tile_size, px // tile_size))

    pygame.display.flip()
