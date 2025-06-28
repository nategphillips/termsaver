// main.c
// A simple random walk terminal screensaver based on project 9 in chapter 8 of K. N. King's book
// "C Programming: A Modern Approach".

// Copyright (C) 2025 Nathan G. Phillips

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#define COL_WIDTH 3

int kbd_hit(void)
{
    // Enables canonical mode, lots of bit manipulation I don't understand. Ripped directly from
    // https://gist.github.com/vsajip/1864660.

    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

int get_cursor_position(size_t *rows, size_t *cols)
{
    // From the kilo.c tutorial.

    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) exit(EXIT_FAILURE);

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }

    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;

    if (sscanf(&buf[2], "%zu;%zu", rows, cols) != 2) return -1;

    return 0;
}

int get_window_size(size_t *rows, size_t *cols)
{
    // Also from kilo.c.

    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return get_cursor_position(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;

        return 0;
    }
}

int main(void)
{
    int direction, row, col, r, g, b;
    bool up, down, left, right, moved;
    size_t num_rows, num_cols, counter = 1;

    if (get_window_size(&num_rows, &num_cols) == -1) exit(EXIT_FAILURE);
    num_cols /= COL_WIDTH;

    int grid[num_rows][num_cols];
    memset(grid, 0, num_rows * num_cols * sizeof(int));

    // Sleep request for 0.05 seconds.
    struct timespec request = {0, 50000000L};

    srand((unsigned) time(NULL));

    // Clear the screen, move cursor to home position.
    printf("\033[2J\033[H");
    // Make cursor invisible.
    printf("\033[?25l");

    for (;;) {
        // Generate random starting position.
        row = rand() % num_rows;
        col = rand() % num_cols;

        // Generate random colors.
        r = rand() % 256;
        g = rand() % 256;
        b = rand() % 256;

        grid[row][col] = counter++;

        while (counter <= num_rows * num_cols) {
            up = down = left = right = true;
            moved = false;

            // Check array bounds and occupied squares.
            if ((row - 1) < 0 || grid[row - 1][col])
                up = false;
            if ((row + 1) >= (int) num_rows || grid[row + 1][col])
                down = false;
            if ((col - 1) < 0 || grid[row][col - 1])
                left = false;
            if ((col + 1) >= (int) num_cols || grid[row][col + 1])
                right = false;

            // Terminate if all four directions are blocked.
            if (!up && !down && !left && !right)
                break;

            direction = rand() % 4;

            switch (direction) {
                // Fallthroughs are intentional; if a case is hit but the required direction is not
                // available, fall through to the next case.
                case 0:
                    if (up) {
                        grid[--row][col] = counter++;
                        moved = true;
                        break;
                    }
                    // fall through
                case 1:
                    if (down) {
                        grid[++row][col] = counter++;
                        moved = true;
                        break;
                    }
                    // fall through
                case 2:
                    if (left) {
                        grid[row][--col] = counter++;
                        moved = true;
                        break;
                    }
                    // fall through
                case 3:
                    if (right) {
                        grid[row][++col] = counter++;
                        moved = true;
                        break;
                    }
                    // fall through
                default:
                    // Don't increment current integer or position; cycle back through and get a new
                    // direction until an available direction is hit.
                    break;
            }

            // If a move is made, only update the cell which was changed.
            if (moved) {
                // Add offset since ANSI codes are 1-indexed.
                printf("\033[%d;%dH", row + 1, col * COL_WIDTH + 1);
                // Set foreground color as RGB.
                printf("\033[38;2;%d;%d;%dm", r, g, b);
                printf("███");
                fflush(stdout);

                nanosleep(&request, NULL);

                if (kbd_hit()) {
                    // Clear the screen, move cursor to home position.
                    printf("\033[2J\033[H");
                    // Make cursor visible.
                    printf("\033[?25h");

                    exit(EXIT_SUCCESS);
                }
            }
        }

        // Clear the previous grid and reset the color.
        printf("\033[2J\033[H");
        printf("\033[0m");

        nanosleep(&request, NULL);

        // Reset the grid and the counter.
        for (size_t i = 0; i < num_rows; i++) {
            for (size_t j = 0; j < num_cols; j++)
                grid[i][j] = 0;
        }

        counter = 1;
    }

    exit(EXIT_FAILURE);
}

