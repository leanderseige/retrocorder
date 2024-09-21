#!/bin/bash

sudo apt update


sudo apt update
sudo apt install libsdl2-dev libsdl2-ttf-dev libportaudio2 libportaudio-dev portaudio19-dev libsndfile1-dev libinih-dev
sudo apt install libsdl2-dev libsdl2-ttf-dev libportaudio2 libportaudio-dev portaudio19-dev libsndfile1-dev libinih-dev



git clone https://github.com/PortAudio/portaudio.git
cd portaudio
./configure
make
sudo make install

