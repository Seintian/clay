#define CLAY_IMPLEMENTATION
#include "../../clay.h"
#include "../../renderers/ncurses/clay_renderer_ncurses.c"
#include <time.h> // for nanosleep

#define DEFAULT_SCROLL_DELTA 3.0f
#define BLACK_BG_COLOR {20, 20, 20, 255}

// State for the example
typedef struct {
    bool sidebarOpen;
    float scrollDelta;
    bool showHelp;
    bool shouldQuit;
} AppState;

static AppState appState = { 
    .sidebarOpen = true, 
    .scrollDelta = 0.0f, 
    .shouldQuit = false, 
    .showHelp = false
};

void HandleInput() {
    // Reset delta per frame
    appState.scrollDelta = 0.0f;

    int ch;
    while ((ch = getch()) != ERR) {
        if (ch == 'q' || ch == 'Q') {
            appState.shouldQuit = true;
        }
        if (ch == 's' || ch == 'S') {
            appState.sidebarOpen = !appState.sidebarOpen;
        }
        if (ch == 'h' || ch == 'H') {
            appState.showHelp = !appState.showHelp;
        }
        if (ch == KEY_UP) {
            appState.scrollDelta += DEFAULT_SCROLL_DELTA;
        }
        if (ch == KEY_DOWN) {
            appState.scrollDelta -= DEFAULT_SCROLL_DELTA;
        }
    }
}

void RenderProgressBar(Clay_String label, float percent, Clay_Color color) {
    CLAY(CLAY_ID_LOCAL("ProgressBarWrapper"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = CLAY_NCURSES_CELL_HEIGHT }
    }) {
        CLAY(CLAY_ID_LOCAL("LabelRow"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) }, .layoutDirection = CLAY_LEFT_TO_RIGHT, .childGap = CLAY_NCURSES_CELL_HEIGHT, .childAlignment = {.y = CLAY_ALIGN_Y_CENTER} }
        }) {
            CLAY_TEXT(label, CLAY_TEXT_CONFIG({ .textColor = {200, 200, 200, 255}, .fontSize = 16 }));
        }

        CLAY(CLAY_ID_LOCAL("BarBackground"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(CLAY_NCURSES_CELL_HEIGHT) } },
            .backgroundColor = {40, 40, 40, 255},
            .cornerRadius = {1} 
        }) {
            CLAY(CLAY_ID_LOCAL("BarFill"), {
                .layout = { .sizing = { CLAY_SIZING_PERCENT(percent), CLAY_SIZING_GROW() } },
                .backgroundColor = color,
                .cornerRadius = {1}
            }) {}
        }
    }
}

void RenderServerStatus() {
    CLAY(CLAY_ID("ServerStatus"), {
        .layout = { 
            .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) },
            .padding = {16, 16, 16, 16},
            .childGap = 16,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = {25, 25, 25, 255},
        .border = { .color = {60, 60, 60, 255}, .width = {2, 2, 2, 2} }
    }) {
        CLAY_TEXT(CLAY_STRING("SERVER STATUS"), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255} }));
        RenderProgressBar(CLAY_STRING("CPU"), 0.45f, (Clay_Color){0, 200, 0, 255});
        RenderProgressBar(CLAY_STRING("Mem"), 0.82f, (Clay_Color){200, 150, 0, 255});
    }
}

void RenderHelpModal() {
    if (!appState.showHelp) return;

    CLAY(CLAY_ID("HelpModalOverlay"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() }, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER} },
        .floating = { .zIndex = 100, .attachTo = CLAY_ATTACH_TO_ROOT, .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE },
        .backgroundColor = {0, 0, 0, 150}
    }) {
        CLAY(CLAY_ID("HelpModalWindow"), {
            .layout = { 
                .sizing = { CLAY_SIZING_FIXED(CLAY_NCURSES_CELL_WIDTH * 60), CLAY_SIZING_FIT(0) },
                .padding = CLAY_PADDING_ALL(CLAY_NCURSES_CELL_HEIGHT),
                .childGap = CLAY_NCURSES_CELL_WIDTH,
                .layoutDirection = CLAY_TOP_TO_BOTTOM
            },
            .backgroundColor = {30, 30, 30, 255},
            .cornerRadius = {4},
            .border = { .color = {255, 255, 255, 255}, .width = {2, 2, 2, 2} }
        }) {
            CLAY_TEXT(CLAY_STRING("Ncurses Example Help"), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255} }));

            CLAY(CLAY_ID("HelpLine1"), { .layout = { .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0)} } }) {
                CLAY_TEXT(CLAY_STRING("Keys:"), CLAY_TEXT_CONFIG({ .textColor = {200, 200, 0, 255} }));
            }
            CLAY_TEXT(CLAY_STRING("- ARROW KEYS: Scroll Feed"), CLAY_TEXT_CONFIG({ .textColor = {200, 200, 200, 255} }));
            CLAY_TEXT(CLAY_STRING("- S: Toggle Sidebar"), CLAY_TEXT_CONFIG({ .textColor = {200, 200, 200, 255} }));
            CLAY_TEXT(CLAY_STRING("- H: Toggle This Help"), CLAY_TEXT_CONFIG({ .textColor = {200, 200, 200, 255} }));
            CLAY_TEXT(CLAY_STRING("- Q: Quit Application"), CLAY_TEXT_CONFIG({ .textColor = {200, 200, 200, 255} }));

            CLAY(CLAY_ID("HelpCloseTip"), { .layout = { .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0)}, .padding = {.top = 16} } }) {
                CLAY_TEXT(CLAY_STRING("Press 'H' to close."), CLAY_TEXT_CONFIG({ .textColor = {100, 100, 100, 255} }));
            }
        }
    }
}

void RenderSidebar() {
    if (!appState.sidebarOpen) return;

    CLAY(CLAY_ID("Sidebar"), {
        .layout = { 
            .sizing = { CLAY_SIZING_FIXED(CLAY_NCURSES_CELL_WIDTH * 30), CLAY_SIZING_GROW() },
            .padding = CLAY_PADDING_ALL(CLAY_NCURSES_CELL_HEIGHT),
            .childGap = CLAY_NCURSES_CELL_HEIGHT,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = {20, 20, 20, 255},
        .border = { .color = {100, 100, 100, 255}, .width = { .right = 2 } } // Lighter Grey Border
    }) {
        CLAY_TEXT(CLAY_STRING("SIDEBAR"), CLAY_TEXT_CONFIG({ 
            .textColor = {255, 255, 0, 255} // Bright Yellow
        }));

        RenderServerStatus();

        CLAY(CLAY_ID("SidebarItem1"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(CLAY_NCURSES_CELL_HEIGHT * 2) } },
            .backgroundColor = BLACK_BG_COLOR
        }) {
            CLAY_TEXT(CLAY_STRING(" > Item 1 🌍"), CLAY_TEXT_CONFIG({ .textColor = {0, 255, 255, 255} }));
        }

        CLAY(CLAY_ID("SidebarItem2"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(CLAY_NCURSES_CELL_HEIGHT * 2) } },
            .backgroundColor = BLACK_BG_COLOR
        }) {
            CLAY_TEXT(CLAY_STRING(" > Item 2 🌐"), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255} }));
        }
        CLAY(CLAY_ID("SidebarItemMixed1"), {
            .layout = { 
                .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(CLAY_NCURSES_CELL_HEIGHT * 3) },
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
            },
            .backgroundColor = {20, 20, 20, 255},
            .cornerRadius = { .topLeft = 8 },
            .border = { .color = {255, 100, 100, 255}, .width = {2, 2, 2, 2} }
        }) {
            CLAY_TEXT(CLAY_STRING(" > TL Round"), CLAY_TEXT_CONFIG({ .textColor = {255, 100, 100, 255} }));
        }

        CLAY(CLAY_ID("SidebarItemMixed2"), {
            .layout = { 
                .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(CLAY_NCURSES_CELL_HEIGHT * 3) },
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
            },
            .backgroundColor = {20, 20, 20, 255},
            .cornerRadius = { .topLeft = 8, .bottomRight = 8 },
            .border = { .color = {100, 255, 100, 255}, .width = {2, 2, 2, 2} }
        }) {
            CLAY_TEXT(CLAY_STRING(" > Diagonal"), CLAY_TEXT_CONFIG({ .textColor = {100, 255, 100, 255} }));
        }

        CLAY(CLAY_ID("SidebarItemMixed3"), {
            .layout = { 
                .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(CLAY_NCURSES_CELL_HEIGHT * 3) },
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
            },
            .backgroundColor = {20, 20, 20, 255},
            .cornerRadius = { .topLeft = 8, .topRight = 8 },
            .border = { .color = {100, 100, 255, 255}, .width = {2, 2, 2, 2} }
        }) {
            CLAY_TEXT(CLAY_STRING(" > Top Round"), CLAY_TEXT_CONFIG({ .textColor = {100, 100, 255, 255} }));
        }
    }
}

// Helpers for "Realistic" Data
const char* NAMES[] = {
    "Alice",
    "Bob",
    "Charlie",
    "Diana",
    "Ethan",
    "Fiona",
    "George",
    "Hannah"
};
const char* TITLES[] = {
    "Just released a new library!",
    "Thoughts on C programming?",
    "Check out this cool algorithm",
    "Why I love Ncurses",
    "Clay UI is pretty flexible",
    "Debugging segfaults all day...",
    "Coffee break time ☕",
    "Anyone going to the conf?"
};
const char* LOREM[] = {
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit.",
    "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.",
    "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.",
    "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum.",
    "Excepteur sint occaecat cupidatat non proident, sunt in culpa."
};

void RenderPost(int index) {
    CLAY(CLAY_IDI("Post", index), {
        .layout = { 
            .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) },
            .padding = CLAY_PADDING_ALL(CLAY_NCURSES_CELL_HEIGHT),
            .childGap = CLAY_NCURSES_CELL_HEIGHT,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = BLACK_BG_COLOR,
        .cornerRadius = {1},
        .border = { .color = {80, 80, 80, 255}, .width = {2, 2, 2, 2} }
    }) {
        // Post Header: Avatar + Name + Time
        CLAY(CLAY_IDI("PostHeader", index), {
            .layout = { 
                .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) },
                .childGap = CLAY_NCURSES_CELL_WIDTH * 2,
                .childAlignment = { .y = CLAY_ALIGN_Y_TOP },
                .layoutDirection = CLAY_LEFT_TO_RIGHT
            }
        }) {
            // Avatar
            CLAY(CLAY_IDI("Avatar", index), {
                .layout = { .sizing = { CLAY_SIZING_FIXED(CLAY_NCURSES_CELL_WIDTH * 4), CLAY_SIZING_FIXED(CLAY_NCURSES_CELL_HEIGHT * 2) } },
                .backgroundColor = { (index * 50) % 255, (index * 80) % 255, (index * 30) % 255, 255 },
                .cornerRadius = {1}
            }) {}

            // Name & Title
            CLAY(CLAY_IDI("AuthorInfo", index), {
                .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 0 }
            }) {
                Clay_String name = { .length = strlen(NAMES[index % 8]), .chars = NAMES[index % 8] };
                Clay_String title = { .length = strlen(TITLES[index % 8]), .chars = TITLES[index % 8] };
                CLAY_TEXT(name, CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255} }));
                CLAY_TEXT(title, CLAY_TEXT_CONFIG({ .textColor = {150, 150, 150, 255} }));
            }
        }

        // Post Body
        CLAY(CLAY_IDI("PostBody", index), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) }, .padding = { .top = CLAY_NCURSES_CELL_HEIGHT, .bottom = CLAY_NCURSES_CELL_HEIGHT } }
        }) {
            Clay_String lorem = { .length = strlen(LOREM[index % 5]), .chars = LOREM[index % 5] };
            CLAY_TEXT(lorem, CLAY_TEXT_CONFIG({ .textColor = {200, 200, 200, 255} }));
        }

        // Post Actions
        CLAY(CLAY_IDI("PostActions", index), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) }, .childGap = CLAY_NCURSES_CELL_HEIGHT, .layoutDirection = CLAY_LEFT_TO_RIGHT }
        }) {
            CLAY_TEXT(CLAY_STRING("[ Like ]"), CLAY_TEXT_CONFIG({ .textColor = {0, 255, 0, 255} })); // Bright Green
            CLAY_TEXT(CLAY_STRING("[ Comment ]"), CLAY_TEXT_CONFIG({ .textColor = {0, 100, 255, 255} })); // Bright Blue
            CLAY_TEXT(CLAY_STRING("[ Share ]"), CLAY_TEXT_CONFIG({ .textColor = {255, 0, 0, 255} })); // Bright Red
        }
    }
}

void RenderContent() {
    CLAY(CLAY_ID("ContentArea"), {
        .layout = { 
            .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
            .padding = CLAY_PADDING_ALL(CLAY_NCURSES_CELL_HEIGHT),
            .childGap = CLAY_NCURSES_CELL_HEIGHT,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = BLACK_BG_COLOR
    }) {
        // Sticky Header
        CLAY(CLAY_ID("Header"), {
            .layout = { 
                .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(CLAY_NCURSES_CELL_HEIGHT * 3) }, // 3 cells high
                .padding = { .left = CLAY_NCURSES_CELL_WIDTH * 2, .right=CLAY_NCURSES_CELL_WIDTH * 2 },
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
            },
            .backgroundColor = BLACK_BG_COLOR,
            .border = { .color = {0, 100, 255, 255}, .width = { .bottom = 1 } }
        }) {
            CLAY_TEXT(CLAY_STRING("Clay Social Feed"), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255} }));
        }

        // Scroll Viewport
        // We use SIZING_GROW for height so it fills the remaining screen space.
        CLAY(CLAY_ID("Viewport"), {
            .layout = { 
                .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() }, 
                .padding = { .top = 8, .bottom = 8 }
            },
            .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() },
            .backgroundColor = BLACK_BG_COLOR
        }) {
            CLAY(CLAY_ID("FeedList"), {
                .layout = { 
                    .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT(0) }, // Fit height to content (allows it to be taller than viewport)
                    .childGap = 16, 
                    .layoutDirection = CLAY_TOP_TO_BOTTOM 
                }
            }) {
                // Get first item pos if possible
                Clay_ElementData item0 = Clay_GetElementData(CLAY_IDI("Post", 0));

                for (int i = 0; i < 50; ++i) { // 50 Posts
                    RenderPost(i);
                }

                CLAY_TEXT(CLAY_STRING("--- End of Feed ---"), CLAY_TEXT_CONFIG({ .textColor = {140, 140, 140, 255} }));
            }
        }

        CLAY_TEXT(CLAY_STRING("Controls: ARROW UP/DOWN to Scroll | Q to Quit | S to Toggle Sidebar"), CLAY_TEXT_CONFIG({ .textColor = {120, 120, 120, 255} }));
    }
}

void RenderMainLayout() {
    CLAY(CLAY_ID("Root"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() }, .layoutDirection = CLAY_LEFT_TO_RIGHT },
    }) {
        RenderSidebar();
        RenderContent();
        RenderHelpModal();
    }
}

int main() {
    uint32_t minMemory = Clay_MinMemorySize();
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(minMemory, malloc(minMemory));

    Clay_Initialize(arena, (Clay_Dimensions){0,0}, (Clay_ErrorHandler){NULL});
    Clay_SetMeasureTextFunction(Clay_Ncurses_MeasureText, NULL);
    Clay_Ncurses_Initialize();

    // Non-blocking input
    timeout(0); 

    while(!appState.shouldQuit) {
        HandleInput();

        Clay_Dimensions dims = Clay_Ncurses_GetLayoutDimensions();
        Clay_SetLayoutDimensions(dims);

        // Handle Scroll Logic
        Clay_ElementId viewportId = CLAY_ID("Viewport");
        Clay_ElementData viewportData = Clay_GetElementData(viewportId);

        if (viewportData.found) {
            Clay_Vector2 center = { 
                viewportData.boundingBox.x + viewportData.boundingBox.width / 2,
                viewportData.boundingBox.y + viewportData.boundingBox.height / 2
            };
            Clay_SetPointerState(center, false);

            Clay_UpdateScrollContainers(true, (Clay_Vector2){0, appState.scrollDelta}, 0.016f);
        }

        Clay_BeginLayout();
        RenderMainLayout();
        Clay_RenderCommandArray commands = Clay_EndLayout();

        Clay_Ncurses_Render(commands);

        struct timespec ts = { .tv_sec = 0, .tv_nsec = 16000 * 1000 };
        nanosleep(&ts, NULL); 
    }

    Clay_Ncurses_Terminate();
    free(arena.memory);
    return 0;
}
