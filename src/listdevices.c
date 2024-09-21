#include <portaudio.h>
#include <stdio.h>

int main(void) {
    Pa_Initialize();
    int numDevices = Pa_GetDeviceCount();
    const PaDeviceInfo *deviceInfo;
    
    printf("Number of devices = %d\n", numDevices);

    for (int i = 0; i < numDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        printf("Device %d: %s\n", i, deviceInfo->name);
        printf("    Max Input Channels: %d\n", deviceInfo->maxInputChannels);
        printf("    Max Output Channels: %d\n", deviceInfo->maxOutputChannels);
        printf("    Default Sample Rate: %.2f\n", deviceInfo->defaultSampleRate);
    }

    Pa_Terminate();
    return 0;
}
