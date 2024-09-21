#!/bin/bash


gcc -o retrocorder src/retrocorder.c -I/usr/include/SDL2 -I/usr/local/include -lSDL2 -lSDL2_ttf -lportaudio -lsndfile -lpthread -linih

gcc -o listdevices src/listdevices.c -I/usr/include/portaudio19 -lportaudio

