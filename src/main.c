#include "raylib.h"
#include "raymedia.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <math.h>

#define CIRCLE_COUNT 40

#ifndef _DEBUG

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

#endif

// Screen states
typedef enum {
    SCREEN_PALETTES,
    SCREEN_START,
    SCREEN_EXPLORER,
    SCREEN_VIEWING,
} ScreenState;

typedef struct {
    char name[64];
    Color lightColor;
    Color darkColor;
    bool selected;
} Palette;

// File types
// File type "Back" is for navigating "back", or navigating to the parent directory, whatever you want to call it.
typedef enum {
    FILE_TYPE_FOLDER,
    FILE_TYPE_VIDEO,
    FILE_TYPE_BACK
} FileType;

// File entry
typedef struct {
    char name[256];
    char path[512];
    FileType type;
    bool selected;
} FileEntry;

// Easing function (ease out cubic)
float EaseOutCubic(float t) {
    return 1 - powf(1 - t, 3);
}

typedef struct {
    unsigned char data[8];
} Sprite;

typedef struct {
    Sprite folder;
    Sprite back;
    Sprite video;
    Sprite play;
    Sprite pause;
    Sprite mute;
    Sprite unmute;
    Sprite loop;
    Sprite unloop;
    Sprite palette;
} Sprites;

// Background circle
typedef struct {
    Vector2 basePos;
    float radius;
    float parallaxFactor;
    float opacity;
} BackgroundCircle;

// Global state
typedef struct {
    ScreenState currentScreen;
    ScreenState targetScreen;

    // Transitions
    float transitionTimer;
    float transitionDuration;
    bool transitioning;
    float screenFadeTimer;
    float screenFadeInDuration;

    // Camera
    Vector2 cameraPos;
    Vector2 targetCameraPos;
    float cameraTransitionTimer;

    // Background
    BackgroundCircle circles[CIRCLE_COUNT];
    int circleCount;
    float circleOpacityTimer;

    // Shaders
    // ditherShader is the backbone.
    // invertShader is for when the colors are flipped for dark mode.
    // We don't want the video to play with an ugly inverted color scheme.
    Shader ditherShader;
    Shader videoShader;

    RenderTexture2D renderTarget;
    RenderTexture2D videoTarget;
    Palette* colorPalettes;
    int colorCount;
    int colorIndex;
    bool flipColors;
    Color lightColor;
    Color darkColor;

    // Sprites
    Sprites sprites;

    // Explorer
    FileEntry* files;
    int fileCount;
    char currentPath[512];
    char pathInput[512];
    int selectedFileIndex;
    float scrollOffset;
    float targetScrollOffset;

    // Video
    char selectedVideoPath[512];
    MediaStream video;
    bool videoLoaded;
    Rectangle videoDisplayRect;
    bool looping;
    bool muted;

    // Rendering
    bool isRendering;
    float renderProgress;

    // Window
    int windowWidth;
    int windowHeight;
} AppState;

AppState state = {0};

// sorry, didn't feel like exporting these and dealing with
// more resources to share in the final project.
// i drew them up in aseprite, then hand-wrote it to some sort of
// weird byte form.
// at least only a human would do something this silly.
void InitSprites() {
    state.sprites.video = (Sprite){0b11111111, 0b10101011, 0b11111111, 0b10000001, 0b10000001, 0b11111111, 0b11010101, 0b11111111};
    state.sprites.folder = (Sprite){0b00001111, 0b11110001, 0b10011111, 0b10000001, 0b10000001, 0b10000001, 0b10000001, 0b11111111};
    state.sprites.back = (Sprite){0b00000000, 0b00100000, 0b01100000, 0b11111111, 0b11111111, 0b01100000, 0b00100000, 0b00000000};
    state.sprites.palette = (Sprite){0b11111000, 0b11111000, 0b11111000, 0b11111111, 0b11111001, 0b00010001, 0b00010001, 0b00011111};
    state.sprites.play = (Sprite){0b11100000, 0b11111000, 0b11111110, 0b11111111, 0b11111111, 0b11111110, 0b11111000, 0b11100000};
    state.sprites.pause = (Sprite){0b01100110,0b01100110,0b01100110,0b01100110,0b01100110,0b01100110,0b01100110,0b01100110};
    state.sprites.loop = (Sprite){0b00111100, 0b01000010, 0b10000001, 0b00000011, 0b11000000, 0b10000001, 0b01000010, 0b00111100};
    state.sprites.unloop = (Sprite){0b00111101, 0b01000010, 0b10000101, 0b00001011, 0b11010000, 0b10100001, 0b01000010, 0b10111100};
    state.sprites.unmute = (Sprite){0b00110000, 0b01110001, 0b01110101, 0b11110101, 0b11110101, 0b01110101, 0b01110001, 0b00110000};
    state.sprites.mute = (Sprite){0b00110000, 0b01110000, 0b01110000, 0b11110000, 0b11110000, 0b01110000, 0b01110000, 0b00110000};
}


void InitPalettes() {
    Palette barePalette = (Palette){"Classic", (Color){255, 255, 255, 255}, (Color){0, 0, 0, 255}};
    Palette gbPalette = (Palette){"Gaming Male Child", (Color){136, 192, 112, 255}, (Color){8, 24, 32, 255}};
    Palette sepiaPalette = (Palette){"Seppy", (Color){214, 169, 86, 255}, (Color){27, 7, 1, 255}};
    Palette bombpopPalette = (Palette){"There's a Bomb in my Pop", (Color){104, 141, 242, 255}, (Color){46, 11, 7, 255}};
    Palette catpuccinPalette = (Palette){"Kitty Coffee", (Color){243, 190, 231, 255}, (Color){26, 23, 39, 255}};
    // New Palette can go here.

    state.colorCount = 5; // Edit this number based on amount of palettes above.
    state.colorPalettes = (Palette*)MemAlloc(sizeof(Palette) * state.colorCount);

    state.colorPalettes[0] = barePalette;
    state.colorPalettes[1] = gbPalette;
    state.colorPalettes[2] = sepiaPalette;
    state.colorPalettes[3] = bombpopPalette;
    state.colorPalettes[4] = catpuccinPalette;
    // And add it to the color palette array here!
}


void InitBackgroundCircles() {
    state.circleCount = CIRCLE_COUNT;
    for (int i = 0; i < state.circleCount; i++) {
        state.circles[i].basePos = (Vector2){
                GetRandomValue(-200, 2560 + 200),
                GetRandomValue(-200, 1440 + 200)
        };
        state.circles[i].radius = GetRandomValue(50, 300);
        state.circles[i].parallaxFactor = 0.1f + (float)GetRandomValue(0, 100) / 500.0f;
        state.circles[i].opacity = 0.0f;
    }
}


void LoadDirectory(const char* path) {
    // Free previous files
    if (state.files != NULL) {
        free(state.files);
        state.files = NULL;
    }
    state.fileCount = 0;
    state.selectedFileIndex = -1;

    strcpy(state.currentPath, path);
    strcat(state.currentPath, "\0");
    strcpy(state.pathInput, state.currentPath);

    FilePathList filePathList = LoadDirectoryFiles(state.currentPath);

    char* filePath;
    for (int i = 0; i < filePathList.count; i++) {
        filePath = filePathList.paths[i];
        if (DirectoryExists(filePath) || IsFileExtension(filePath, ".mov") || IsFileExtension(filePath, ".mp4") ||
                IsFileExtension(filePath, ".mkv") || IsFileExtension(filePath, ".avi")) {
            state.fileCount++;
        }
    }

    state.files = (FileEntry*)MemAlloc(sizeof(FileEntry) * (state.fileCount + 1));

    strcpy(state.files[0].name, "Navigate to parent directory..\0");
    strcpy(state.files[0].path, GetPrevDirectoryPath(state.currentPath));
    state.files[0].selected = false;
    state.files[0].type = FILE_TYPE_BACK;
    int fileIndex = 1;


    // Import directories first before implementing standalone files (so directories and files aren't intermixed).
    for (int i = 0; i < filePathList.count; i++) {
        filePath = filePathList.paths[i];
        if (DirectoryExists(filePath)) {
            strcpy(state.files[fileIndex].name, GetFileName(filePath));
            strcpy(state.files[fileIndex].path, filePath);
            state.files[fileIndex].type = FILE_TYPE_FOLDER;
            state.files[fileIndex++].selected = false;
        }
    }

    // Finally, implement files.
    for (int i = 0; i < filePathList.count; i++) {
        filePath = filePathList.paths[i];
        if (IsFileExtension(filePath, ".mov") || IsFileExtension(filePath, ".mp4") ||
            IsFileExtension(filePath, ".mkv") || IsFileExtension(filePath, ".avi")) {
            strcpy(state.files[fileIndex].name, GetFileName(filePath));
            strcpy(state.files[fileIndex].path, filePath);
            state.files[fileIndex].type = FILE_TYPE_VIDEO;
            state.files[fileIndex++].selected = false;
        }
    }

    state.fileCount = fileIndex;
    UnloadDirectoryFiles(filePathList);

    state.scrollOffset = 0;
    state.targetScrollOffset = 0;
}


void TransitionToScreen(ScreenState screen) {
    state.targetScreen = screen;
    state.transitioning = true;
    state.transitionTimer = 0;
    state.transitionDuration = 0.5f;

    // Set target camera position with ease-in from below
    state.targetCameraPos.x = screen * 200.0f;
    state.targetCameraPos.y = 0;

    state.cameraTransitionTimer = 0;
}


void DrawBackgroundCircles(float fadeAlpha) {
    Vector2 mousePos = GetMousePosition();
    Vector2 mouseDelta = {
            (mousePos.x - state.windowWidth / 2) * 0.02f,
            (mousePos.y - state.windowHeight / 2) * 0.02f
    };

    for (int i = 0; i < state.circleCount; i++) {
        Vector2 pos = {
                state.circles[i].basePos.x - state.cameraPos.x * state.circles[i].parallaxFactor +
                mouseDelta.x * state.circles[i].parallaxFactor,
                state.circles[i].basePos.y - state.cameraPos.y * state.circles[i].parallaxFactor +
                mouseDelta.y * state.circles[i].parallaxFactor
        };

        // Use gray color for circles with combined opacity
        Color circleColor = (Color){0, 0, 0, (unsigned char)(state.circles[i].opacity * fadeAlpha * 255)};
        DrawCircle((int)pos.x, (int)pos.y, state.circles[i].radius, circleColor);
    }
}

bool DrawButton(Rectangle bounds, const char* text, float* hoverScale, float alpha) {
    Vector2 mousePos = GetMousePosition();
    bool isHovered = CheckCollisionPointRec(mousePos, bounds);
    bool isClicked = false;

    // Update hover scale
    float targetScale = isHovered ? 1.1f : 1.0f;
    *hoverScale += (targetScale - *hoverScale) * 0.2f;

    // Calculate scaled bounds
    Rectangle scaledBounds = {
            bounds.x + bounds.width * (1 - *hoverScale) / 2,
            bounds.y + bounds.height * (1 - *hoverScale) / 2,
            bounds.width * *hoverScale,
            bounds.height * *hoverScale
    };

    if (isHovered) {
        // Shadow
        Color shadowColor = (Color){0, 0, 0, (unsigned char)(25 * alpha)};
        if (!state.flipColors) DrawRectangleRounded((Rectangle){
                scaledBounds.x, scaledBounds.y + 10,
                scaledBounds.width, scaledBounds.height
        }, 0.2f, 8, shadowColor);

        // White outline (by drawing a larger rectangle behind the fill)
        Color whiteOutline = (Color){255, 255, 255, (unsigned char)(255 * alpha)};
        DrawRectangleRounded((Rectangle){scaledBounds.x - 3, scaledBounds.y - 3, scaledBounds.width + 6, scaledBounds.height + 6},
                             0.3f, 8, whiteOutline);

        // Gray fill
        Color grayFill = (Color){150, 150, 150, (unsigned char)(255 * alpha)};
        DrawRectangleRounded(scaledBounds, 0.2f, 8, grayFill);



        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            isClicked = true;
        }
    } else {
        // White fill
        Color whiteFill = (Color){255, 255, 255, (unsigned char)(255 * alpha)};
        DrawRectangleRounded(scaledBounds, 0.2f, 8, whiteFill);

        // Black outline
        Color blackOutline = (Color){0, 0, 0, (unsigned char)(255 * alpha)};
        DrawRectangleRoundedLines(scaledBounds, 0.2f, 8, blackOutline);
    }

    // Text
    int fontSize = 20;
    Vector2 textSize = MeasureTextEx(GetFontDefault(), text, fontSize, 1);
    Vector2 textPos = {
            scaledBounds.x + (scaledBounds.width - textSize.x) / 2,
            scaledBounds.y + (scaledBounds.height - textSize.y) / 2
    };

    if (isHovered) {
        // White outline around text
        Color whiteOutline = (Color){255, 255, 255, (unsigned char)(255 * alpha)};
        for (int dx = -2; dx <= 2; dx++) {
            for (int dy = -2; dy <= 2; dy++) {
                if (dx != 0 || dy != 0) {
                    DrawTextEx(GetFontDefault(), text,
                               (Vector2){textPos.x + dx, textPos.y + dy},
                               fontSize, 1, whiteOutline);
                }
            }
        }
    }

    Color textColor = (Color){0, 0, 0, (unsigned char)(255 * alpha)};
    DrawTextEx(GetFontDefault(), text, textPos, fontSize, 1, textColor);

    return isClicked;
}

bool DrawSpriteButton(Rectangle bounds, Sprite icon, float* hoverScale, float alpha) {
    Vector2 mousePos = GetMousePosition();
    bool isHovered = CheckCollisionPointRec(mousePos, bounds);
    bool isClicked = false;

    // Update hover scale
    float targetScale = isHovered ? 1.1f : 1.0f;
    *hoverScale += (targetScale - *hoverScale) * 0.2f;

    // Calculate scaled bounds
    Rectangle scaledBounds = {
            bounds.x + bounds.width * (1 - *hoverScale) / 2,
            bounds.y + bounds.height * (1 - *hoverScale) / 2,
            bounds.width * *hoverScale,
            bounds.height * *hoverScale
    };

    if (isHovered) {
        // Shadow
        Color shadowColor = (Color){0, 0, 0, (unsigned char)(25 * alpha)};
        if (!state.flipColors) DrawRectangleRounded((Rectangle){
                    scaledBounds.x, scaledBounds.y + 10,
                    scaledBounds.width, scaledBounds.height
            }, 0.2f, 8, shadowColor);

        // White outline (by drawing a larger rectangle behind the fill)
        Color whiteOutline = (Color){255, 255, 255, (unsigned char)(255 * alpha)};
        DrawRectangleRounded((Rectangle){scaledBounds.x - 3, scaledBounds.y - 3, scaledBounds.width + 6, scaledBounds.height + 6},
                             0.3f, 8, whiteOutline);

        // Gray fill
        Color grayFill = (Color){150, 150, 150, (unsigned char)(255 * alpha)};
        DrawRectangleRounded(scaledBounds, 0.2f, 8, grayFill);


        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            isClicked = true;
        }
    } else {
        // White fill
        Color whiteFill = (Color){255, 255, 255, (unsigned char)(255 * alpha)};
        DrawRectangleRounded(scaledBounds, 0.2f, 8, whiteFill);

        // Black outline
        Color blackOutline = (Color){0, 0, 0, (unsigned char)(255 * alpha)};
        DrawRectangleRoundedLines(scaledBounds, 0.2f, 8, blackOutline);
    }

    // Icon
    Vector2 iconPos = (Vector2){bounds.x + (bounds.width / 2) - 8, bounds.y + (bounds.height / 2) - 8};
    for (int row = 0; row < 8; row++) {
        for (int bit = 0; bit < 8; bit++) {
            int pixelOn = (icon.data[row] >> (7 - bit)) & 1;
            Color color = pixelOn ? ((isHovered) ? WHITE : BLACK) : ColorAlpha(WHITE, 0);
            DrawRectangle(iconPos.x + 2*bit, iconPos.y + 2*row, 2, 2, color);
        }
    }

    return isClicked;
}


void SetNewColors(Shader* shader, Color lightColor, Color darkColor) {
    state.lightColor = lightColor;
    state.darkColor = darkColor;

    if (state.ditherShader.id > 0) {
        int darkColorLoc = GetShaderLocation(*shader, "darkColor");
        int lightColorLoc = GetShaderLocation(*shader, "lightColor");

        if (darkColorLoc != -1 && lightColorLoc != -1) {
            float darkColorVec[4] = {darkColor.r/256.0, darkColor.g/256.0, darkColor.b/256.0, 1};
            float lightColorVec[4] = {lightColor.r/256.0, lightColor.g/256.0, lightColor.b/256.0, 1};
            SetShaderValue(*shader, darkColorLoc, darkColorVec, SHADER_UNIFORM_VEC4);
            SetShaderValue(*shader, lightColorLoc, lightColorVec, SHADER_UNIFORM_VEC4);
        } else {
            TraceLog(LOG_WARNING, "Shader uniform locations not found");
        }
    } else {
        TraceLog(LOG_WARNING, "Colors unchanged as shader was not loaded.");
    }
}


void InitApp() {
    state.windowWidth = 1280;
    state.windowHeight = 720;
    state.looping = true;

    InitPalettes();
    InitSprites();

    InitWindow(state.windowWidth, state.windowHeight, "FlipFilter");
    InitAudioDevice();
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));

    // Load shader (check if it loaded successfully)
    state.ditherShader = LoadShader(NULL, "dither.fs");
    state.videoShader = LoadShader(NULL, "dither.fs");
    state.flipColors = false;

    // Create render texture
    state.renderTarget = LoadRenderTexture(state.windowWidth, state.windowHeight);
    state.videoTarget = LoadRenderTexture(state.windowWidth, state.windowHeight);

    // Only set shader uniforms if shader loaded successfully
    state.colorIndex = 2;
    SetNewColors(&state.ditherShader, state.colorPalettes[state.colorIndex].lightColor, state.colorPalettes[state.colorIndex].darkColor);
    SetNewColors(&state.videoShader, state.colorPalettes[state.colorIndex].lightColor, state.colorPalettes[state.colorIndex].darkColor);

    // Initialize background
    InitBackgroundCircles();

    // Start screen transition
    state.currentScreen = SCREEN_START;
    state.targetScreen = SCREEN_START;
    state.cameraPos = (Vector2){state.currentScreen * 200, 100};
    state.targetCameraPos = (Vector2){state.currentScreen * 200, 0};
    state.circleOpacityTimer = 0;
    state.screenFadeTimer = 0;
    state.screenFadeInDuration = 1.0f;
    state.transitionTimer = 0;
    state.transitionDuration = 0.5f;
    state.transitioning = true;

    // Get current directory
    getcwd(state.currentPath, sizeof(state.currentPath));
    LoadDirectory(state.currentPath);
}


void UpdateApp() {
    // Handle window resize
    if (IsWindowResized()) {
        state.windowWidth = GetScreenWidth();
        state.windowHeight = GetScreenHeight();

        // Reload render texture with new size
        UnloadRenderTexture(state.renderTarget);
        state.renderTarget = LoadRenderTexture(state.windowWidth, state.windowHeight);

        UnloadRenderTexture(state.videoTarget);
        state.videoTarget = LoadRenderTexture(state.windowWidth, state.windowHeight);
    }

    // Update camera position
    state.cameraTransitionTimer += GetFrameTime();
    float cameraT = fminf(state.cameraTransitionTimer / 1.0f, 1.0f);
    float easedT = EaseOutCubic(cameraT);

    state.cameraPos.x += (state.targetCameraPos.x - state.cameraPos.x) * easedT * 0.1f;
    state.cameraPos.y += (state.targetCameraPos.y - state.cameraPos.y) * easedT * 0.1f;

    // Update circle opacity with fade in
    if (state.circleOpacityTimer < 1.0f) {
        state.circleOpacityTimer += GetFrameTime() / 1.5f;
        float opacity = EaseOutCubic(fminf(state.circleOpacityTimer, 1.0f));
        for (int i = 0; i < state.circleCount; i++) {
            state.circles[i].opacity = opacity * 0.3f;
        }
    }

    // Update screen fade in
    if (state.screenFadeTimer < state.screenFadeInDuration) {
        state.screenFadeTimer += GetFrameTime();
    }

    // Update smooth scroll
    state.scrollOffset += (state.targetScrollOffset - state.scrollOffset) * 0.2f;

    // Handle transitions
    if (state.transitioning) {
        state.transitionTimer += GetFrameTime();
        if (state.transitionTimer >= state.transitionDuration) {
            state.currentScreen = state.targetScreen;
            state.transitioning = false;

            state.scrollOffset = 0;
            state.targetScrollOffset = 0;
        }
    }
}


void DrawStartScreen() {
    float alpha = EaseOutCubic(fminf(state.transitionTimer / state.transitionDuration, 1.0f));
    if (state.transitioning && state.currentScreen == SCREEN_START && state.targetScreen != SCREEN_START) alpha = 1 - alpha;

    // Title
    const char* title = "FlipFilter";
    int fontSize = 60;
    Vector2 titleSize = MeasureTextEx(GetFontDefault(), title, fontSize, 2);
    Vector2 titlePos = {
            (state.windowWidth - titleSize.x) / 2,
            fminf(state.windowHeight / 3, state.windowHeight / 2 - 10 - fontSize - 4 - 10)
    };

    Color titleColor = BLACK;
    titleColor.a = (unsigned char)(alpha * 255);
    for (int dx = -4; dx <= 4; dx += 2) {
        for (int dy = -4; dy <= 4; dy += 2) {
            DrawTextEx(GetFontDefault(), title, (Vector2){titlePos.x + dx, titlePos.y + dy}, fontSize, 2, ColorAlpha(WHITE, alpha));
        }
    }
    DrawTextEx(GetFontDefault(), title, titlePos, fontSize, 2, ColorAlpha(BLACK, alpha));

    // Button
    static float browseHoverScale = 1.0f;
    static float paletteHoverScale = 1.0f;
    Rectangle browseBounds = {
            state.windowWidth / 2 - 100,
            state.windowHeight / 2 + 50,
            200, 50
    };
    Rectangle paletteBounds = {
            state.windowWidth / 2 - 100,
            state.windowHeight / 2 - 10,
            200, 50
    };

    if (DrawButton(paletteBounds, "Change Palette", &paletteHoverScale, alpha) && alpha > 0.9f) {
        TransitionToScreen(SCREEN_PALETTES);
    }
    if (DrawButton(browseBounds, "Browse Files..", &browseHoverScale, alpha) && alpha > 0.9f) {
        TransitionToScreen(SCREEN_EXPLORER);
    }
}


void DrawExplorerScreen() {
    float alpha = EaseOutCubic(fminf(state.transitionTimer / state.transitionDuration, 1.0f));
    if (state.transitioning && state.currentScreen == SCREEN_EXPLORER) alpha = 1 - alpha;

    // Path input at top
    Rectangle pathInputRect = {20, 20, state.windowWidth - 40, 40};

    DrawRectangleRounded(pathInputRect, 0.2f, 8, ColorAlpha(WHITE, alpha));
    DrawRectangleRoundedLines(pathInputRect, 0.2f, 8, ColorAlpha(BLACK, alpha));
    BeginScissorMode(30, 30, state.windowWidth - 60, 40);
    DrawText(state.pathInput, (MeasureText(state.pathInput, 20) > state.windowWidth - 60) ? 30 - (MeasureText(state.pathInput, 20) - (state.windowWidth - 60)) : 30, 30, 20, ColorAlpha(BLACK, alpha));
    EndScissorMode();

    // View button
    static float viewHoverScale = 1.0f;
    Rectangle viewButton = {state.windowWidth - 220, state.windowHeight - 80, 200, 50};
    if (state.selectedFileIndex >= 0 &&
        state.files[state.selectedFileIndex].type == FILE_TYPE_VIDEO &&
        DrawButton(viewButton, "View Media", &viewHoverScale, alpha) && alpha > 0.9f && !state.transitioning) {
        strcpy(state.selectedVideoPath, state.files[state.selectedFileIndex].path);
        state.videoLoaded = false;
        TransitionToScreen(SCREEN_VIEWING);
    }

    // Back button
    static float backHoverScale = 1.0f;
    Rectangle backButton = {20, state.windowHeight - 80, 200, 50};
    if (DrawButton(backButton, "Back to Home", &backHoverScale, alpha) && alpha > 0.9f) {
        TransitionToScreen(SCREEN_START);
    }

    // File list
    float listY = 80;
    float listHeight = state.windowHeight - 180;
    float itemHeight = 50;

    Rectangle listBounds = {20, listY, state.windowWidth - 40, listHeight};
    BeginScissorMode((int)listBounds.x, (int)listBounds.y,
                     (int)listBounds.width, (int)listBounds.height);

    for (int i = 0; i < state.fileCount; i++) {
        float y = listY + i * itemHeight + state.scrollOffset;
        Rectangle itemRect = {listBounds.x, y, listBounds.width, itemHeight};

        bool isHovered = CheckCollisionPointRec(GetMousePosition(), itemRect);

        // Background
        if (state.files[i].selected) {
            DrawRectangleRec(itemRect, ColorAlpha(GRAY, alpha));
        } else {
            DrawRectangleRec(itemRect, ColorAlpha(WHITE, alpha));
        }

        // Separator line
        DrawLineEx((Vector2){itemRect.x, itemRect.y + itemHeight},
                   (Vector2){itemRect.x + itemRect.width, itemRect.y + itemHeight},
                   2, ColorAlpha(BLACK, alpha));

        // Icon and text
        Sprite icon;
        switch (state.files[i].type) {
            case FILE_TYPE_VIDEO:
                icon = state.sprites.video;
                break;
            case FILE_TYPE_BACK:
                icon = state.sprites.back;
                break;
            case FILE_TYPE_FOLDER:
                icon = state.sprites.folder;
                break;
            default:
                break;
        }

        // Draw icon
        Vector2 iconPos = (Vector2){itemRect.x + 20, itemRect.y + 16};
        for (int row = 0; row < 8; row++) {
            for (int bit = 0; bit < 8; bit++) {
                int pixelOn = (icon.data[row] >> (7 - bit)) & 1;
                Color color = pixelOn ? ((state.files[i].selected) ? WHITE : BLACK) : ColorAlpha(WHITE, 0);
                DrawRectangle(iconPos.x + 2*bit, iconPos.y + 2*row, 2, 2, color);
            }
        }

        // Text with outline if selected
        if (state.files[i].selected) {
            Color whiteOutlineColor = (Color){255, 255, 255, (unsigned char)(255 * alpha)};
            for (int dx = -2; dx <= 2; dx += 2) {
                for (int dy = -2; dy <= 2; dy += 2) {
                    if (dx != 0 || dy != 0) {
                        DrawText(state.files[i].name,
                                 (int)itemRect.x + 50 + dx,
                                 (int)itemRect.y + 15 + dy, 20, whiteOutlineColor);
                    }
                }
            }
        }
        DrawText(state.files[i].name, (int)itemRect.x + 50, (int)itemRect.y + 15, 20, ColorAlpha(BLACK, alpha));

        // Handle clicks (only when fully faded in)
        if (alpha > 0.9f && isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (state.selectedFileIndex == i) {
                // Double click
                if (state.files[i].type != FILE_TYPE_VIDEO) {
                    LoadDirectory(state.files[i].path);
                }
            } else {
                // Select
                for (int j = 0; j < state.fileCount; j++) {
                    state.files[j].selected = false;
                }
                state.files[i].selected = true;
                state.selectedFileIndex = i;
            }
        }
    }

    EndScissorMode();

    // Scroll handling
    float mouseWheel = GetMouseWheelMove();
    if (mouseWheel != 0) {
        state.targetScrollOffset += mouseWheel * 30;
        state.targetScrollOffset = fmaxf(state.targetScrollOffset,
                                         -(state.fileCount * itemHeight - listHeight));
        state.targetScrollOffset = fminf(state.targetScrollOffset, 0);
    }

    // List outline
    DrawRectangleLinesEx(listBounds, 2, ColorAlpha(BLACK, alpha));
}


void DrawViewScreen() {
    float alpha = EaseOutCubic(fminf(state.transitionTimer / state.transitionDuration, 1.0f));
    if (state.transitioning && state.currentScreen == SCREEN_VIEWING) alpha = 1 - alpha;

    if (!state.videoLoaded && strlen(state.selectedVideoPath) > 0) {
        UnloadRenderTexture(state.videoTarget);
        state.videoTarget = LoadRenderTexture(state.windowWidth, state.windowHeight);
        UnloadMedia(&state.video);
        state.video = LoadMedia(state.selectedVideoPath);
        if (state.muted) SetAudioStreamVolume(state.video.audioStream, 0.0f);
        state.videoLoaded = true;
        if (state.looping) SetMediaLooping(state.video, true);
    }

    Rectangle bounds = {20, 20, state.windowWidth - 40, state.windowHeight - 120};

    if (state.videoLoaded) {
        UpdateMedia(&state.video);

        // Calculate video display rectangle maintaining aspect ratio
        float videoAspect = (float)state.video.videoTexture.width / state.video.videoTexture.height;
        float boundsAspect = bounds.width / bounds.height;

        if (videoAspect > boundsAspect) {
            state.videoDisplayRect.width = bounds.width;
            state.videoDisplayRect.height = bounds.width / videoAspect;
            state.videoDisplayRect.x = bounds.x;
            state.videoDisplayRect.y = bounds.y + (bounds.height - state.videoDisplayRect.height) / 2;
        } else {
            state.videoDisplayRect.height = bounds.height;
            state.videoDisplayRect.width = bounds.height * videoAspect;
            state.videoDisplayRect.x = bounds.x + (bounds.width - state.videoDisplayRect.width) / 2;
            state.videoDisplayRect.y = bounds.y;
        }

        DrawRectangleRec((Rectangle){state.videoDisplayRect.x - 4, state.videoDisplayRect.y - 4, state.videoDisplayRect.width + 8, state.videoDisplayRect.height + 8}, (state.flipColors) ? ColorAlpha(WHITE, alpha) : ColorAlpha(BLACK, alpha));
    }

    // Explorer button
    static float applyHoverScale = 1.0f;
    Rectangle applyButton = {state.windowWidth - 220, state.windowHeight - 80, 200, 50};
    if (DrawButton(applyButton, "View Another", &applyHoverScale, alpha) && alpha > 0.9f && !state.transitioning) {
        TransitionToScreen(SCREEN_EXPLORER);
    }

    static float playHoverScale = 1.0f;
    Rectangle playButton = {state.windowWidth / 2 - 20, state.videoDisplayRect.y + state.videoDisplayRect.height + 20,40, 40};
    if (DrawSpriteButton(playButton, ((GetMediaState(state.video) == MEDIA_STATE_PLAYING) ? state.sprites.pause : state.sprites.play), &playHoverScale, alpha)) {
        if (GetMediaState(state.video) == MEDIA_STATE_PLAYING) {
            SetMediaState(state.video, MEDIA_STATE_PAUSED);
        } else {
            SetMediaState(state.video, MEDIA_STATE_PLAYING);
        }
    }

    static float loopHoverScale = 1.0f;
    Rectangle loopButton = {state.windowWidth / 2 - 20 + 60, state.videoDisplayRect.y + state.videoDisplayRect.height + 20,40, 40};
    if (DrawSpriteButton(loopButton, (state.looping ? state.sprites.loop : state.sprites.unloop), &loopHoverScale, alpha)) {
        state.looping = !state.looping;
        SetMediaLooping(state.video, state.looping);
    }

    static float muteHoverScale = 1.0f;
    Rectangle muteButton = {state.windowWidth / 2 - 20 - 60, state.videoDisplayRect.y + state.videoDisplayRect.height + 20,40, 40};
    if (DrawSpriteButton(muteButton, (state.muted ? state.sprites.mute : state.sprites.unmute), &muteHoverScale, alpha)) {
        state.muted = !state.muted;
        SetAudioStreamVolume(state.video.audioStream, (state.muted) ? 0.0f : 1.0f);
    }
}

void DrawPalettesScreen() {
    float alpha = EaseOutCubic(fminf(state.transitionTimer / state.transitionDuration, 1.0f));
    if (state.transitioning && state.currentScreen == SCREEN_PALETTES) alpha = 1 - alpha;

    // Borrowing list rectangle from explorer
    float listY = 20;
    float listHeight = state.windowHeight - 120;
    float itemHeight = 50;

    Rectangle listBounds = {20, listY, state.windowWidth - 40, listHeight};
    BeginScissorMode((int)listBounds.x, (int)listBounds.y,
                     (int)listBounds.width, (int)listBounds.height);
    for (int i = 0; i < state.colorCount; i++) {
        float y = listY + i * itemHeight + state.scrollOffset;
        Rectangle itemRect = {listBounds.x, y, listBounds.width, itemHeight};

        bool isHovered = CheckCollisionPointRec(GetMousePosition(), itemRect);

        // Background
        if (state.colorPalettes[i].selected) {
            Color grayBg = (Color) {150, 150, 150, (unsigned char) (255 * alpha)};
            DrawRectangleRec(itemRect, grayBg);
        } else {
            Color whiteBg = (Color) {255, 255, 255, (unsigned char) (255 * alpha)};
            DrawRectangleRec(itemRect, whiteBg);
        }

        // Separator line
        Color lineColor = (Color) {0, 0, 0, (unsigned char) (255 * alpha)};
        DrawLineEx((Vector2) {itemRect.x, itemRect.y + itemHeight},
                   (Vector2) {itemRect.x + itemRect.width, itemRect.y + itemHeight},
                   2, lineColor);

        // Draw icon
        Vector2 iconPos = (Vector2){itemRect.x + 20, itemRect.y + 16};
        for (int row = 0; row < 8; row++) {
            for (int bit = 0; bit < 8; bit++) {
                int pixelOn = (state.sprites.palette.data[row] >> (7 - bit)) & 1;
                Color color = pixelOn ? ((state.colorPalettes[i].selected) ? WHITE : BLACK) : ColorAlpha(WHITE, 0);
                DrawRectangle(iconPos.x + 2*bit, iconPos.y + 2*row, 2, 2, color);
            }
        }

        // Text with outline if selected
        if (state.colorPalettes[i].selected) {
            Color whiteOutlineColor = (Color){255, 255, 255, (unsigned char)(255 * alpha)};
            for (int dx = -2; dx <= 2; dx += 2) {
                for (int dy = -2; dy <= 2; dy += 2) {
                    if (dx != 0 || dy != 0) {
                        DrawText(state.colorPalettes[i].name,
                                 (int)itemRect.x + 50 + dx,
                                 (int)itemRect.y + 15 + dy, 20, whiteOutlineColor);
                    }
                }
            }
        }
        DrawText(state.colorPalettes[i].name, (int)itemRect.x + 50, (int)itemRect.y + 15, 20, ColorAlpha(BLACK, alpha));

        // Handle clicks (only when fully faded in)
        if (alpha > 0.9f && isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (!state.colorPalettes[i].selected) {
                if (state.flipColors) SetNewColors(&state.ditherShader, state.colorPalettes[i].darkColor, state.colorPalettes[i].lightColor);
                else SetNewColors(&state.ditherShader, state.colorPalettes[i].lightColor, state.colorPalettes[i].darkColor);
                SetNewColors(&state.videoShader, state.colorPalettes[i].lightColor, state.colorPalettes[i].darkColor);
                state.colorIndex = i;
            }
        }

        if (state.colorIndex != i) {
            state.colorPalettes[i].selected = false;
        } else {
            state.colorPalettes[i].selected = true;
        }
    }
    EndScissorMode();

    static float backHoverScale = 1.0f;
    Rectangle backButton = {state.windowWidth -200 - 20, state.windowHeight - 80, 200, 50};
    if (DrawButton(backButton, "Back to Home", &backHoverScale, alpha) && alpha > 0.9f) {
        TransitionToScreen(SCREEN_START);
    }

    static float flipHoverScale = 1.0f;
    Rectangle flipButton = {20, state.windowHeight - 80, 200, 50};
    if (DrawButton(flipButton, (!state.flipColors) ? "Light Mode" : "Dark Mode", &flipHoverScale, alpha) && alpha > 0.9f) {
        state.flipColors = !state.flipColors;
        if (state.flipColors) {
            SetNewColors(&state.ditherShader, state.colorPalettes[state.colorIndex].darkColor, state.colorPalettes[state.colorIndex].lightColor);
        }
        else {
            SetNewColors(&state.ditherShader, state.colorPalettes[state.colorIndex].lightColor, state.colorPalettes[state.colorIndex].darkColor);
        }
    }

    // Scroll handling
    float mouseWheel = GetMouseWheelMove();
    if (mouseWheel != 0) {
        state.targetScrollOffset += mouseWheel * 30;
        state.targetScrollOffset = fmaxf(state.targetScrollOffset,
                                         -(state.colorCount * itemHeight - listHeight));
        state.targetScrollOffset = fminf(state.targetScrollOffset, 0);
    }

    // List outline
    DrawRectangleLinesEx(listBounds, 2, ColorAlpha(BLACK, alpha));
}


void RenderApp() {
    float alpha = EaseOutCubic(fminf(state.screenFadeTimer / state.screenFadeInDuration, 1.0f));

    float videoAlpha = EaseOutCubic(fminf(state.transitionTimer / state.transitionDuration, 1.0f));
    if (state.transitioning && state.currentScreen == SCREEN_VIEWING) videoAlpha = 1 - videoAlpha;

    BeginTextureMode(state.renderTarget);
    ClearBackground((Color){220, 220, 220, 255});

    DrawBackgroundCircles(alpha);

    if (state.transitioning) {
        switch (state.targetScreen) {
            case SCREEN_START:
                DrawStartScreen();
                break;
            case SCREEN_EXPLORER:
                DrawExplorerScreen();
                break;
            case SCREEN_VIEWING:
                DrawViewScreen();
                break;
            case SCREEN_PALETTES:
                DrawPalettesScreen();
                break;
        }
    }

    // Draw current screen
    switch (state.currentScreen) {
        case SCREEN_START:
            DrawStartScreen();
            break;
        case SCREEN_EXPLORER:
            DrawExplorerScreen();
            break;
        case SCREEN_VIEWING:
            DrawViewScreen();
            break;
        case SCREEN_PALETTES:
            DrawPalettesScreen();
            break;
    }

    EndTextureMode();

    if (state.videoLoaded && (state.currentScreen == SCREEN_VIEWING || state.targetScreen == SCREEN_VIEWING)) {
        BeginTextureMode(state.videoTarget);
        BeginShaderMode(state.videoShader);
        DrawTexturePro(state.video.videoTexture,
                       (Rectangle){0, 0, (float)state.video.videoTexture.width, (float)state.video.videoTexture.height},
                       state.videoDisplayRect, (Vector2){0, 0}, 0, WHITE);
        EndShaderMode();
        EndTextureMode();
    }

    BeginDrawing();
    BeginShaderMode(state.ditherShader);
    DrawTextureRec(state.renderTarget.texture,
                   (Rectangle){0, 0, state.renderTarget.texture.width, -state.renderTarget.texture.height},
                   (Vector2){0, 0}, WHITE);
    EndShaderMode();
    if (state.videoLoaded && (state.currentScreen == SCREEN_VIEWING || state.targetScreen == SCREEN_VIEWING))
        DrawTextureRec(state.videoTarget.texture, (Rectangle){0, 0, state.videoTarget.texture.width, -state.videoTarget.texture.height}, (Vector2){0, 0}, ColorAlpha(WHITE, videoAlpha));
    EndDrawing();
}


int main() {
    InitApp();

    while (!WindowShouldClose()) {
        UpdateApp();
        RenderApp();
    }

    if (state.videoLoaded) {
        UnloadMedia(&state.video);
    }
    if (state.files != NULL) {
        free(state.files);
    }
    // There's some cleanup probably missing here, sorry.
    // Closing the application covers most of our bases anyway :P
    UnloadRenderTexture(state.renderTarget);
    UnloadRenderTexture(state.videoTarget);
    UnloadShader(state.ditherShader);
    CloseWindow();

    return 0;
}