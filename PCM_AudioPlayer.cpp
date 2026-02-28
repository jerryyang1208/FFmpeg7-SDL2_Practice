#include <iostream>
#include <fstream> 
#include <vector>
#include <string>
#include <windows.h> // 必须引入，用于调用 Windows API
#include <commdlg.h> // 必须引入，用于文件打开对话框

extern "C" {
    #include <SDL.h>
}

#define AUDIO_BUFFER_SIZE 4096 

// 函数：调用 Windows API 弹出文件选择窗口
std::string SelectFileDialog() {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    // 设置过滤器，方便筛选 pcm 文件
    ofn.lpstrFilter = "PCM Audio Files\0*.pcm\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(szFile);
    }
    return "";
}

// 播放 PCM 音频的核心函数
void play_pcm(const std::string& filepath) {
    if (filepath.empty()) return;

    std::ifstream file(filepath, std::ios::binary); 
    if (!file.is_open()) {
        std::cerr << "Could not open PCM file: " << filepath << std::endl;
        return;
    }

    std::vector<Uint8> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close(); 

    if (buffer.empty()) {
        std::cerr << "PCM file is empty or invalid." << std::endl;
        return;
    }

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return;
    }

    SDL_AudioSpec wanted_spec;
    SDL_AudioSpec obtained_spec; 
    wanted_spec.freq = 44100;                 // 注意：这里假设你选择的所有 PCM 都是 44.1kHz
    wanted_spec.format = AUDIO_S16SYS;        
    wanted_spec.channels = 2;                 // 注意：这里假设你选择的所有 PCM 都是双声道
    wanted_spec.samples = AUDIO_BUFFER_SIZE;  
    wanted_spec.callback = nullptr;           

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &obtained_spec, 0);
    if (dev == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    if (SDL_QueueAudio(dev, buffer.data(), buffer.size()) < 0) {
        std::cerr << "Failed to queue audio: " << SDL_GetError() << std::endl;
        SDL_CloseAudioDevice(dev);
        SDL_Quit();
        return;
    }

    std::cout << "Now playing: " << filepath << std::endl;
    SDL_PauseAudioDevice(dev, 0);

    while (SDL_GetQueuedAudioSize(dev) > 0) {
        SDL_Delay(100); 
    }

    SDL_CloseAudioDevice(dev);
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    SDL_SetMainReady(); 

    // 1. 弹出对话框选择文件
    std::string selectedPath = SelectFileDialog();

    // 2. 如果用户选择了文件（没点取消），则进行播放
    if (!selectedPath.empty()) {
        play_pcm(selectedPath);
    } else {
        std::cout << "No file selected. Exiting..." << std::endl;
    }

    return 0;
}