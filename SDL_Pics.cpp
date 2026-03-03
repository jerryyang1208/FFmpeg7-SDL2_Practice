#include <iostream>
extern "C" {
    #include <SDL.h>
}

// 窗口尺寸
const int WINDOW_WIDTH = 640;
const int WINDOW_HEIGHT = 480;

// 定义要绘制的图形（使用常量，避免全局变量）
const SDL_Rect bigRect = {50, 50, 300, 200};      // 蓝色大矩形
const SDL_Rect smallRect = {400, 300, 100, 100};  // 青色小矩形
const SDL_Point trianglePoints[4] = {             // 白色三角形（闭合线条）
    {320, 200},
    {300, 240},
    {340, 240},
    {320, 200}
};
const int pointCount = 4;

// 初始化 SDL 窗口和渲染器
bool initSDL(SDL_Window*& window, SDL_Renderer*& renderer) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL 初始化失败: " << SDL_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow("SDL2 绘图示例",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH,
                              WINDOW_HEIGHT,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "窗口创建失败: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "渲染器创建失败: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    return true;
}

// 清理资源
void cleanup(SDL_Window* window, SDL_Renderer* renderer) {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

// 绘制所有图形
void render(SDL_Renderer* renderer) {
    // 清空屏幕为红色背景
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);

    // 绘制蓝色大矩形
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderFillRect(renderer, &bigRect);

    // 绘制白色三角形（线条）
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLines(renderer, trianglePoints, pointCount);

    // 绘制青色小矩形
    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
    SDL_RenderFillRect(renderer, &smallRect);

    // 更新屏幕显示
    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    if (!initSDL(window, renderer)) {
        return 1;
    }

    // 主循环标志
    bool running = true;
    SDL_Event event;

    while (running) {
        // 处理事件队列
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;  // 点击窗口关闭按钮时退出循环
            }
        }

        // 渲染画面（由于图形不变，可以每帧重绘；若想节省CPU可加入延时）
        render(renderer);

        // 简单的帧率控制（防止CPU占用过高）
        SDL_Delay(16);  // 约60 FPS
    }

    cleanup(window, renderer);
    return 0;
}