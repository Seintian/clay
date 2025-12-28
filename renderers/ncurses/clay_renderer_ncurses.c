#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <wchar.h>
#include "../../clay.h"

// -------------------------------------------------------------------------------------------------
// -- Internal State & Context
// -------------------------------------------------------------------------------------------------

#define CLAY_NCURSES_CELL_WIDTH 8.0f
#define CLAY_NCURSES_CELL_HEIGHT 16.0f

static int _clayNcursesScreenWidth = 0;
static int _clayNcursesScreenHeight = 0;
static bool _clayNcursesInitialized = false;

// Scissor / Clipping State
#define MAX_SCISSOR_STACK_DEPTH 16
static Clay_BoundingBox _scissorStack[MAX_SCISSOR_STACK_DEPTH];
static int _scissorStackIndex = 0;

// Color State
// We reserve pair 0. Pairs 1..max are dynamically allocated.
#define MAX_COLOR_PAIRS_CACHE 1024
static struct {
    short fg;
    short bg;
    int pairId;
} _colorPairCache[MAX_COLOR_PAIRS_CACHE];
static int _colorPairCacheSize = 0;

// -------------------------------------------------------------------------------------------------
// -- Forward Declarations
// -------------------------------------------------------------------------------------------------

static short Clay_Ncurses_GetColorId(Clay_Color color);
static int Clay_Ncurses_GetColorPair(short fg, short bg);
static bool Clay_Ncurses_IntersectScissor(int x, int y, int w, int h, int *outX, int *outY, int *outW, int *outH);
static void Clay_Ncurses_InitLocale(void);
static int Clay_Ncurses_MeasureStringWidth(Clay_StringSlice text);
static void Clay_Ncurses_RenderText(Clay_StringSlice text, int x, int y, int renderWidth);

static short Clay_Ncurses_GetBackgroundAt(int x, int y) {
    chtype ch = mvinch(y, x);
    int pair = PAIR_NUMBER(ch);
    short fg, bg;
    pair_content(pair, &fg, &bg);
    return bg;
}

// -------------------------------------------------------------------------------------------------
// -- Public API Implementation
// -------------------------------------------------------------------------------------------------

void Clay_Ncurses_Initialize() {
    if (_clayNcursesInitialized) return;

    Clay_Ncurses_InitLocale();
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

    start_color();
    use_default_colors();

    getmaxyx(stdscr, _clayNcursesScreenHeight, _clayNcursesScreenWidth);

    _scissorStack[0] = (Clay_BoundingBox){0, 0, (float)_clayNcursesScreenWidth * CLAY_NCURSES_CELL_WIDTH, (float)_clayNcursesScreenHeight * CLAY_NCURSES_CELL_HEIGHT};
    _scissorStackIndex = 0;

    _clayNcursesInitialized = true;
}

void Clay_Ncurses_Terminate() {
    if (_clayNcursesInitialized) {
        clear();
        refresh();
        endwin();

        SCREEN *s = set_term(NULL);
        if (s) {
            delscreen(s);
        }

        _clayNcursesInitialized = false;
    }
}

Clay_Dimensions Clay_Ncurses_GetLayoutDimensions() {
    return (Clay_Dimensions) {
        .width = (float)_clayNcursesScreenWidth * CLAY_NCURSES_CELL_WIDTH,
        .height = (float)_clayNcursesScreenHeight * CLAY_NCURSES_CELL_HEIGHT
    };
}

Clay_Dimensions Clay_Ncurses_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    (void)config;
    (void)userData;

    int width = Clay_Ncurses_MeasureStringWidth(text);
    return (Clay_Dimensions) {
        .width = (float)width * CLAY_NCURSES_CELL_WIDTH,
        .height = CLAY_NCURSES_CELL_HEIGHT
    };
}

void Clay_Ncurses_Render(Clay_RenderCommandArray renderCommands) {
    if (!_clayNcursesInitialized) return;

    int newW, newH;
    getmaxyx(stdscr, newH, newW);
    if (newW != _clayNcursesScreenWidth || newH != _clayNcursesScreenHeight) {
        _clayNcursesScreenWidth = newW;
        _clayNcursesScreenHeight = newH;
    }

    _scissorStack[0] = (Clay_BoundingBox){0, 0, (float)_clayNcursesScreenWidth * CLAY_NCURSES_CELL_WIDTH, (float)_clayNcursesScreenHeight * CLAY_NCURSES_CELL_HEIGHT};
    _scissorStackIndex = 0;

    for (int i = 0; i < renderCommands.length; i++) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&renderCommands, i);
        Clay_BoundingBox box = command->boundingBox;

        switch (command->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                int x = (int)(box.x / CLAY_NCURSES_CELL_WIDTH);
                int y = (int)(box.y / CLAY_NCURSES_CELL_HEIGHT);
                int w = (int)(box.width / CLAY_NCURSES_CELL_WIDTH);
                int h = (int)(box.height / CLAY_NCURSES_CELL_HEIGHT);
                int dx, dy, dw, dh;
                if (!Clay_Ncurses_IntersectScissor(x, y, w, h, &dx, &dy, &dw, &dh)) continue;

                short fg = Clay_Ncurses_GetColorId(command->renderData.rectangle.backgroundColor);
                short bg = fg;
                int pair = Clay_Ncurses_GetColorPair(fg, bg);

                chtype targetChar = ' ' | COLOR_PAIR(pair);
                for (int row = dy; row < dy + dh; row++) {
                    for (int col = dx; col < dx + dw; col++) {
                        // Robust dirty check: Mask out attributes like A_BOLD which we don't control but might be set by terminal defaults
                        chtype current = mvinch(row, col);
                        if ((current & (A_CHARTEXT | A_COLOR)) != (targetChar & (A_CHARTEXT | A_COLOR))) {
                            mvaddch(row, col, targetChar);
                        }
                    }
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                int x = (int)(box.x / CLAY_NCURSES_CELL_WIDTH);
                int y = (int)(box.y / CLAY_NCURSES_CELL_HEIGHT);
                Clay_StringSlice text = command->renderData.text.stringContents;
                int textWidth = Clay_Ncurses_MeasureStringWidth(text);

                int dx, dy, dw, dh;
                if (!Clay_Ncurses_IntersectScissor(x, y, textWidth, 1, &dx, &dy, &dw, &dh)) continue;

                short fg = Clay_Ncurses_GetColorId(command->renderData.text.textColor);
                short bg = Clay_Ncurses_GetBackgroundAt(dx, dy);

                int pair = Clay_Ncurses_GetColorPair(fg, bg);

                attron(COLOR_PAIR(pair));

                int skipCols = dx - x;
                int takeCols = dw;

                int maxLen = text.length + 1;
                wchar_t *wbuf = (wchar_t *)malloc(maxLen * sizeof(wchar_t));
                if (wbuf) {
                    char *tempC = (char *)malloc(text.length + 1);
                    memcpy(tempC, text.chars, text.length);
                    tempC[text.length] = '\0';

                    int wlen = mbstowcs(wbuf, tempC, maxLen);
                    free(tempC);

                    if (wlen != -1) {
                        int currentCols = 0;
                        int startIdx = 0;
                        int endIdx = 0;

                        for (int k = 0; k < wlen; k++) {
                            int cw = wcwidth(wbuf[k]);
                            if (cw < 0) cw = 0;

                            if (currentCols >= skipCols) {
                                startIdx = k;
                                break;
                            }
                            currentCols += cw;
                            startIdx = k + 1;
                        }

                        currentCols = 0;
                        int col = 0;
                        int printStart = -1;
                        int printLen = 0;

                        for (int k = 0; k < wlen; k++) {
                            int cw = wcwidth(wbuf[k]);
                            if (cw < 0) cw = 0;

                            if (col >= skipCols && col < skipCols + takeCols) {
                                if (printStart == -1) printStart = k;
                                printLen++;
                            } else if (col < skipCols && col + cw > skipCols) {
                                // Overlap start boundary (e.g. half of a wide char?)
                            }

                            col += cw;
                            if (col >= skipCols + takeCols) break;
                        }

                        if (printStart != -1) {
                            cchar_t *screenChars = (cchar_t *)malloc((printLen + 8) * sizeof(cchar_t));
                            if (screenChars) {
                                int readCount = mvin_wchnstr(dy, dx, screenChars, printLen);
                                if (readCount == ERR) readCount = 0;

                                bool dirty = false;
                                if (readCount < printLen) dirty = true;
                                else {
                                    for (int i = 0; i < printLen; i++) {
                                        wchar_t wch_screen[10] = {0}; 
                                        attr_t attrs;
                                        short color_pair;
                                        if (getcchar(&screenChars[i], wch_screen, &attrs, &color_pair, NULL) == ERR) {
                                            dirty = true;
                                            break;
                                        }

                                        if (wch_screen[0] != wbuf[printStart + i]) {
                                            dirty = true;
                                            break;
                                        }

                                        if ((int)color_pair != pair) {
                                            dirty = true;
                                            break;
                                        }
                                    }
                                }
                                free(screenChars);

                                if (dirty) {
                                    mvaddnwstr(dy, dx, wbuf + printStart, printLen);
                                }
                            } else {
                                // Fallback if malloc fails
                                mvaddnwstr(dy, dx, wbuf + printStart, printLen);
                            }
                        }
                    }
                    free(wbuf);
                }

                attroff(COLOR_PAIR(pair));
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                int x = (int)(box.x / CLAY_NCURSES_CELL_WIDTH);
                int y = (int)(box.y / CLAY_NCURSES_CELL_HEIGHT);
                int w = (int)(box.width / CLAY_NCURSES_CELL_WIDTH);
                int h = (int)(box.height / CLAY_NCURSES_CELL_HEIGHT);

                int dx, dy, dw, dh;
                if (!Clay_Ncurses_IntersectScissor(x, y, w, h, &dx, &dy, &dw, &dh)) continue;

                short color = Clay_Ncurses_GetColorId(command->renderData.border.color);

                short bg = Clay_Ncurses_GetBackgroundAt(dx, dy);
                int pair = Clay_Ncurses_GetColorPair(color, bg);

                attron(COLOR_PAIR(pair));

                cchar_t wc;
                wchar_t wstr[2]; 

                // Top
                if (y >= dy && y < dy + dh) {
                    int sx = x + 1, sw = w - 2;
                    int lx = (sx > dx) ? sx : dx;
                    int rx = (sx + sw < dx + dw) ? (sx + sw) : (dx + dw);
                    mbstowcs(wstr, "─", 2);
                    for (int i = lx; i < rx; i++) {
                        mvin_wch(y, i, &wc);
                        if (wc.chars[0] != wstr[0]) mvprintw(y, i, "─"); // Only print if different
                    }
                }
                // Bottom
                if (y + h - 1 >= dy && y + h - 1 < dy + dh) {
                    int sx = x + 1, sw = w - 2;
                    int lx = (sx > dx) ? sx : dx;
                    int rx = (sx + sw < dx + dw) ? (sx + sw) : (dx + dw);
                    mbstowcs(wstr, "─", 2);
                    for (int i = lx; i < rx; i++) {
                        mvin_wch(y + h - 1, i, &wc);
                        if (wc.chars[0] != wstr[0]) mvprintw(y + h - 1, i, "─");
                    }
                }
                // Left
                if (x >= dx && x < dx + dw) {
                    int sy = y + 1, sh = h - 2;
                    int ty = (sy > dy) ? sy : dy;
                    int by = (sy + sh < dy + dh) ? (sy + sh) : (dy + dh);
                    mbstowcs(wstr, "│", 2);
                    for (int i = ty; i < by; i++) {
                        mvin_wch(i, x, &wc);
                        if (wc.chars[0] != wstr[0]) mvprintw(i, x, "│");
                    }
                }
                // Right
                if (x + w - 1 >= dx && x + w - 1 < dx + dw) {
                    int sy = y + 1, sh = h - 2;
                    int ty = (sy > dy) ? sy : dy;
                    int by = (sy + sh < dy + dh) ? (sy + sh) : (dy + dh);
                    mbstowcs(wstr, "│", 2);
                    for (int i = ty; i < by; i++) {
                        mvin_wch(i, x + w - 1, &wc);
                        if (wc.chars[0] != wstr[0]) mvprintw(i, x + w - 1, "│");
                    }
                }

                // Corners
                if (x >= dx && x < dx + dw && y >= dy && y < dy + dh) {
                    if (command->renderData.border.cornerRadius.topLeft > 0) mvprintw(y, x, "╭");
                    else mvprintw(y, x, "┌");
                }
                if (x + w - 1 >= dx && x + w - 1 < dx + dw && y >= dy && y < dy + dh) {
                    if (command->renderData.border.cornerRadius.topRight > 0) mvprintw(y, x + w - 1, "╮");
                    else mvprintw(y, x + w - 1, "┐");
                }
                if (x >= dx && x < dx + dw && y + h - 1 >= dy && y + h - 1 < dy + dh) {
                    if (command->renderData.border.cornerRadius.bottomLeft > 0) mvprintw(y + h - 1, x, "╰");
                    else mvprintw(y + h - 1, x, "└");
                }
                if (x + w - 1 >= dx && x + w - 1 < dx + dw && y + h - 1 >= dy && y + h - 1 < dy + dh) {
                    if (command->renderData.border.cornerRadius.bottomRight > 0) mvprintw(y + h - 1, x + w - 1, "╯"); // 
                    else mvprintw(y + h - 1, x + w - 1, "┘");
                }

                attroff(COLOR_PAIR(pair));
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                if (_scissorStackIndex < MAX_SCISSOR_STACK_DEPTH - 1) {
                    Clay_BoundingBox current = _scissorStack[_scissorStackIndex];
                    Clay_BoundingBox next = command->boundingBox;

                    // Intersect next with current
                    float nX = (next.x > current.x) ? next.x : current.x;
                    float nY = (next.y > current.y) ? next.y : current.y;
                    float nR = ((next.x + next.width) < (current.x + current.width)) ? (next.x + next.width) : (current.x + current.width);
                    float nB = ((next.y + next.height) < (current.y + current.height)) ? (next.y + next.height) : (current.y + current.height);

                    _scissorStackIndex++;
                    _scissorStack[_scissorStackIndex] = (Clay_BoundingBox){ nX, nY, nR - nX, nB - nY };
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                if (_scissorStackIndex > 0) {
                    _scissorStackIndex--;
                }
                break;
            }
            default: break;
        }
    }

    refresh();
}

// -------------------------------------------------------------------------------------------------
// -- Internal Helpers
// -------------------------------------------------------------------------------------------------

static bool Clay_Ncurses_IntersectScissor(int x, int y, int w, int h, int *outX, int *outY, int *outW, int *outH) {
    Clay_BoundingBox clip = _scissorStack[_scissorStackIndex];

    // Convert clip to cell coords
    int cx = (int)(clip.x / CLAY_NCURSES_CELL_WIDTH);
    int cy = (int)(clip.y / CLAY_NCURSES_CELL_HEIGHT);
    int cw = (int)(clip.width / CLAY_NCURSES_CELL_WIDTH);
    int ch = (int)(clip.height / CLAY_NCURSES_CELL_HEIGHT);

    // Intersect 
    int ix = (x > cx) ? x : cx;
    int iy = (y > cy) ? y : cy;
    int right = (x + w < cx + cw) ? (x + w) : (cx + cw);
    int bottom = (y + h < cy + ch) ? (y + h) : (cy + ch);

    int iw = right - ix;
    int ih = bottom - iy;

    if (iw <= 0 || ih <= 0) return false;

    *outX = ix;
    *outY = iy;
    *outW = iw;
    *outH = ih;
    return true;
}

static short Clay_Ncurses_MatchColor(Clay_Color color) {
    // If not 256 colors, fallback to 8 colors
    if (COLORS < 256) {
        int r = color.r > 128;
        int g = color.g > 128;
        int b = color.b > 128;

        if (r && g && b) return COLOR_WHITE;
        if (!r && !g && !b) return COLOR_BLACK;
        if (r && g) return COLOR_YELLOW;
        if (r && b) return COLOR_MAGENTA;
        if (g && b) return COLOR_CYAN;
        if (r) return COLOR_RED;
        if (g) return COLOR_GREEN;
        if (b) return COLOR_BLUE;
        return COLOR_WHITE;
    }

    // 256 Color Match
    // 1. Check standard ANSI (0-15) - simplified, usually handled by cube approximation anyway but kept for specific fidelity if needed.

    // 2. 6x6x6 Color Cube (16 - 231)
    // Formula: 16 + (36 * r) + (6 * g) + b
    // where r,g,b are 0-5

    int r = (int)((color.r / 255.0f) * 5.0f);
    int g = (int)((color.g / 255.0f) * 5.0f);
    int b = (int)((color.b / 255.0f) * 5.0f);

    // We can compute distance but mapping to the 0-5 grid is usually "good enough" for TUI
    // For better fidelity we actually map 0-255 to the specific values [0, 95, 135, 175, 215, 255] used in xterm
    // But simple linear 0-5 bucket is standard shortcut.

    // Let's use simple bucket for now.
    int cubeIndex = 16 + (36 * r) + (6 * g) + b;

    // 3. Grayscale (232-255)
    // If r~=g~=b, check if grayscale provides better match?
    // Often cube is fine. Grayscale ramp adds fine detail for darks.

    return (short)cubeIndex;
}

static short Clay_Ncurses_GetColorId(Clay_Color color) {
    return Clay_Ncurses_MatchColor(color);
}

static int Clay_Ncurses_GetColorPair(short fg, short bg) {
    // Check cache
    for (int i = 0; i < _colorPairCacheSize; i++) {
        if (_colorPairCache[i].fg == fg && _colorPairCache[i].bg == bg) {
            return _colorPairCache[i].pairId;
        }
    }

    // Create new
    if (_colorPairCacheSize >= MAX_COLOR_PAIRS_CACHE) {
        // Full? Just return last one or default.
        // Real impl: LRU eviction.
        return 0; // Default
    }

    int newId = _colorPairCacheSize + 1;
    init_pair(newId, fg, bg);

    _colorPairCache[_colorPairCacheSize].fg = fg;
    _colorPairCache[_colorPairCacheSize].bg = bg;
    _colorPairCache[_colorPairCacheSize].pairId = newId;
    _colorPairCacheSize++;

    return newId;
}

static void Clay_Ncurses_InitLocale(void) {
    // Attempt 1: environment locale
    char *locale = setlocale(LC_ALL, "");

    // If environment is non-specific (C or POSIX), try to force a UTF-8 one.
    if (!locale || strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0) {
        // Attempt 2: C.UTF-8 (standard on many modern Linux)
        locale = setlocale(LC_ALL, "C.UTF-8");

        if (!locale) {
            // Attempt 3: en_US.UTF-8 (Common fallback)
            locale = setlocale(LC_ALL, "en_US.UTF-8");
        }
    }
}

static int Clay_Ncurses_MeasureStringWidth(Clay_StringSlice text) {
    // Need temporary null-terminated string for mbstowcs
    // Or iterate bytes with mbtowc
    int width = 0;
    const char *ptr = text.chars;
    int len = text.length;

    // Reset shift state
    mbtowc(NULL, NULL, 0);

    while (len > 0) {
        wchar_t wc;
        int bytes = mbtowc(&wc, ptr, len);
        if (bytes <= 0) {
            // Error or null? skip byte
            ptr++;
            len--;
            continue;
        }
        int w = wcwidth(wc);
        if (w > 0) width += w;
        ptr += bytes;
        len -= bytes;
    }
    return width;
}
