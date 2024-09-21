#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <portaudio.h>
#include <sndfile.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <ini.h>

#define BUFFER_SIZE 48000 * 10
#define FRAMES_PER_BUFFER 512

typedef struct {
    float *buffer;
    int maxFrames;
    int frameIndex;
} AudioData;

typedef struct {
    bool fullscreen;
    int audioDevice;
    int sampleRate;
    int channels;
    int resolution;
    SDL_Color bgColor;
    SDL_Color fgColor;
    SDL_Color recColor;
    SDL_Color timeColor;
    SDL_Color buttonTextColor;
    SDL_Color buttonBorderColor;
    SDL_Color buttonBgColor;
} Config;

AudioData audioData;
Config config;

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
TTF_Font *font = NULL;
SDL_Rect recButton, stopButton, deleteButton, exitButton, recordingLabel, waveformRegion, bottomRegion;
bool recording = false;
bool stopRecording = false;
bool deleteRecording = false;
Uint32 startTime = 0;
float waveform[960];
char savePath[256] = "";
pthread_t recordingThread;
PaDeviceIndex deviceIndex;

static int configHandler(void *user, const char *section, const char *name, const char *value) {
    Config *pconfig = (Config *)user;
    if (strcmp(name, "fullscreen") == 0) pconfig->fullscreen = strcmp(value, "true") == 0;
    else if (strcmp(name, "audioDevice") == 0) pconfig->audioDevice = atoi(value);
    else if (strcmp(name, "sampleRate") == 0) pconfig->sampleRate = atoi(value);
    else if (strcmp(name, "channels") == 0) pconfig->channels = atoi(value);
    else if (strcmp(name, "resolution") == 0) pconfig->resolution = atoi(value);
    else if (strcmp(name, "bgColor") == 0) sscanf(value, "%hhu,%hhu,%hhu", &pconfig->bgColor.r, &pconfig->bgColor.g, &pconfig->bgColor.b);
    else if (strcmp(name, "fgColor") == 0) sscanf(value, "%hhu,%hhu,%hhu", &pconfig->fgColor.r, &pconfig->fgColor.g, &pconfig->fgColor.b);
    else if (strcmp(name, "recColor") == 0) sscanf(value, "%hhu,%hhu,%hhu", &pconfig->recColor.r, &pconfig->recColor.g, &pconfig->recColor.b);
    else if (strcmp(name, "timeColor") == 0) sscanf(value, "%hhu,%hhu,%hhu", &pconfig->timeColor.r, &pconfig->timeColor.g, &pconfig->timeColor.b);
    else if (strcmp(name, "buttonTextColor") == 0) sscanf(value, "%hhu,%hhu,%hhu", &pconfig->buttonTextColor.r, &pconfig->buttonTextColor.g, &pconfig->buttonTextColor.b);
    else if (strcmp(name, "buttonBorderColor") == 0) sscanf(value, "%hhu,%hhu,%hhu", &pconfig->buttonBorderColor.r, &pconfig->buttonBorderColor.g, &pconfig->buttonBorderColor.b);
    else if (strcmp(name, "buttonBgColor") == 0) sscanf(value, "%hhu,%hhu,%hhu", &pconfig->buttonBgColor.r, &pconfig->buttonBgColor.g, &pconfig->buttonBgColor.b);
    return 1;
}

void loadConfig(const char *filename) {
    config.fullscreen = false;
    config.audioDevice = Pa_GetDefaultInputDevice();
    config.sampleRate = 48000;
    config.channels = 1;
    config.resolution = 16;
    config.bgColor = (SDL_Color){0, 30, 0};
    config.fgColor = (SDL_Color){0, 255, 0};
    config.recColor = (SDL_Color){255, 0, 0};
    config.timeColor = (SDL_Color){255, 255, 255};
    config.buttonTextColor = (SDL_Color){0, 255, 0};
    config.buttonBorderColor = (SDL_Color){0, 255, 0};
    config.buttonBgColor = (SDL_Color){0, 50, 0};
    if (ini_parse(filename, configHandler, &config) < 0) printf("Can't load config file: %s\n", filename);
}

static int recordCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
    AudioData *data = (AudioData *)userData;
    const float *in = (const float *)inputBuffer;
    unsigned long i;
    if (inputBuffer == NULL) for (i = 0; i < framesPerBuffer; i++) data->buffer[data->frameIndex++] = 0.0f;
    else for (i = 0; i < framesPerBuffer; i++) if (data->frameIndex < data->maxFrames) data->buffer[data->frameIndex++] = *in++;
    else stopRecording = true;
    return stopRecording ? paComplete : paContinue;
}

void saveAudio(const char *filename) {
    SF_INFO sfinfo;
    sfinfo.channels = config.channels;
    sfinfo.samplerate = config.sampleRate;
    sfinfo.format = (config.resolution == 16) ? (SF_FORMAT_WAV | SF_FORMAT_PCM_16) : (SF_FORMAT_WAV | SF_FORMAT_PCM_24);
    SNDFILE *file = sf_open(filename, SFM_WRITE, &sfinfo);
    if (!file) { printf("Failed to open file: %s\n", sf_strerror(file)); return; }
    sf_write_float(file, audioData.buffer, audioData.frameIndex);
    sf_close(file);
}

void renderTextCentered(const char *message, SDL_Rect rect, SDL_Color color) {
    SDL_Surface *surface = TTF_RenderText_Solid(font, message, color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    int textW = surface->w, textH = surface->h;
    SDL_Rect textRect = {rect.x + (rect.w - textW) / 2, rect.y + (rect.h - textH) / 2, textW, textH};
    SDL_RenderCopy(renderer, texture, NULL, &textRect);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void drawButton(SDL_Rect *button, const char *label) {
    SDL_SetRenderDrawColor(renderer, config.buttonBgColor.r, config.buttonBgColor.g, config.buttonBgColor.b, 255);
    SDL_RenderFillRect(renderer, button);
    SDL_SetRenderDrawColor(renderer, config.buttonBorderColor.r, config.buttonBorderColor.g, config.buttonBorderColor.b, 255);
    SDL_RenderDrawRect(renderer, button);
    renderTextCentered(label, *button, config.buttonTextColor);
}

void drawUI() {
    SDL_SetRenderDrawColor(renderer, config.bgColor.r, config.bgColor.g, config.bgColor.b, 255);
    SDL_RenderClear(renderer);
    drawButton(&recButton, "REC");
    drawButton(&stopButton, "STOP");
    drawButton(&deleteButton, "DELETE");
    drawButton(&exitButton, "EXIT");
    if (strlen(savePath) > 0) renderTextCentered(savePath, recordingLabel, config.fgColor);
    Uint32 elapsed = recording ? (SDL_GetTicks() - startTime) / 1000 : 0;
    char timeText[32];
    snprintf(timeText, 32, "Time: %02d:%02d", elapsed / 60, elapsed % 60);
    renderTextCentered(timeText, bottomRegion, recording ? config.recColor : config.timeColor);
    SDL_RenderPresent(renderer);
}

void updateWaveform() {
    for (int i = 0; i < 960; i++) waveform[i] = audioData.buffer[i * (audioData.frameIndex / 960)] * 100.0f;
    drawUI();
    SDL_SetRenderDrawColor(renderer, config.fgColor.r, config.fgColor.g, config.fgColor.b, 255);
    for (int i = 0; i < 959; i++) SDL_RenderDrawLine(renderer, i, waveformRegion.y + waveform[i], i + 1, waveformRegion.y + waveform[i + 1]);
    SDL_RenderPresent(renderer);
}

void showFullWaveform() {
    SDL_SetRenderDrawColor(renderer, config.fgColor.r, config.fgColor.g, config.fgColor.b, 255);
    for (int i = 0; i < 959; i++) SDL_RenderDrawLine(renderer, i, waveformRegion.y + audioData.buffer[i * (audioData.frameIndex / 960)] * 100.0f, i + 1, waveformRegion.y + audioData.buffer[(i + 1) * (audioData.frameIndex / 960)] * 100.0f);
    SDL_RenderPresent(renderer);
}

void *recordingThreadFunction(void *arg) {
    PaError err;
    PaStream *stream;
    audioData.frameIndex = 0;
    stopRecording = false;
    err = Pa_OpenDefaultStream(&stream, config.channels, 0, paFloat32, config.sampleRate, FRAMES_PER_BUFFER, recordCallback, &audioData);
    if (err != paNoError) { printf("PortAudio error: %s\n", Pa_GetErrorText(err)); return NULL; }
    err = Pa_StartStream(stream);
    if (err != paNoError) { printf("PortAudio error: %s\n", Pa_GetErrorText(err)); return NULL; }
    recording = true;
    startTime = SDL_GetTicks();
    while (Pa_IsStreamActive(stream) == 1 && !stopRecording) SDL_Delay(100);
    err = Pa_CloseStream(stream);
    if (err != paNoError) printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    recording = false;
    return NULL;
}

void startRecording() { pthread_create(&recordingThread, NULL, recordingThreadFunction, NULL); }

bool initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError()); return false; }
    if (TTF_Init() < 0) { printf("TTF could not initialize! TTF_Error: %s\n", TTF_GetError()); return false; }
    font = TTF_OpenFont("/usr/share/fonts/truetype/noto/NotoSansMono-Bold.ttf", 24);
    if (font == NULL) { printf("Failed to load font! TTF_Error: %s\n", TTF_GetError()); return false; }
    
    Uint32 windowFlags = SDL_WINDOW_SHOWN;
    if (config.fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    window = SDL_CreateWindow("Retro Audio Recorder", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 960, 540, windowFlags);
    if (window == NULL) { printf("Window could not be created! SDL_Error: %s\n", SDL_GetError()); return false; }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) { printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError()); return false; }

    // Set the logical size for the renderer to 960x540
    SDL_RenderSetLogicalSize(renderer, 960, 540);  // Maintains proportions in fullscreen

    SDL_SetRenderDrawColor(renderer, config.bgColor.r, config.bgColor.g, config.bgColor.b, 255);
    int screenWidth = 960, screenHeight = 540, regionHeight = screenHeight / 3, buttonWidth = screenWidth / 4 - 10, buttonHeight = buttonWidth / 2;
    recButton = (SDL_Rect){10, 10, buttonWidth, buttonHeight};
    stopButton = (SDL_Rect){screenWidth / 4 + 5, 10, buttonWidth, buttonHeight};
    deleteButton = (SDL_Rect){screenWidth / 2 + 5, 10, buttonWidth, buttonHeight};
    exitButton = (SDL_Rect){3 * screenWidth / 4 + 5, 10, buttonWidth, buttonHeight};
    waveformRegion = (SDL_Rect){0, screenHeight * 2 / 3, screenWidth, regionHeight / 2};
    recordingLabel = (SDL_Rect){10, regionHeight * 2 + 5, 940, (regionHeight / 2) - 10};
    bottomRegion = (SDL_Rect){10, regionHeight * 2 + (regionHeight / 2) + 5, 940, (regionHeight / 2) - 10};

    return true;
}


int main(int argc, char *argv[]) {
    loadConfig("config.ini");
    if (!initSDL()) return -1;
    if (Pa_Initialize() != paNoError) return -1;
    audioData.buffer = (float *)malloc(BUFFER_SIZE * sizeof(float));
    audioData.maxFrames = BUFFER_SIZE;
    bool quit = false;
    SDL_Event event;
    while (!quit) {
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) quit = true;
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                int x = event.button.x, y = event.button.y;
                if (SDL_PointInRect(&(SDL_Point){x, y}, &recButton) && !recording) startRecording();
                else if (SDL_PointInRect(&(SDL_Point){x, y}, &stopButton) && recording) { stopRecording = true; pthread_join(recordingThread, NULL); sprintf(savePath, "recording-%ld.wav", time(NULL)); saveAudio(savePath); showFullWaveform(); }
                else if (SDL_PointInRect(&(SDL_Point){x, y}, &deleteButton)) { deleteRecording = true; strcpy(savePath, ""); memset(audioData.buffer, 0, BUFFER_SIZE * sizeof(float)); }
                else if (SDL_PointInRect(&(SDL_Point){x, y}, &exitButton)) quit = true;
            }
        }
        if (recording) updateWaveform();
        else drawUI();
        SDL_Delay(16);
    }
    free(audioData.buffer);
    TTF_CloseFont(font);
    TTF_Quit();
    Pa_Terminate();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

