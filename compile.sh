#!/bin/bash

gcc -o retrocorder src/retrocorder.c -I/opt/homebrew/include -linih -lSDL2 -lSDL2_ttf -lportaudio -lsndfile -lpthread
gcc -o listdevices src/listdevices.c -lportaudio
