Chrono Rift OS Project

Overview

Chrono Rift is an operating systems semester project built in C++
The project uses separate Arbiter, HIP, and ASP processes ( processes defined according to the the game ) with shared state and process coordination.

Project structure

BCS A 24i 0720 24i 0845 HamzaSheikh arbiter
Arbiter source files and renderer header

BCS A 24i 0720 24i 0845 HamzaSheikh hip
HIP process source files

BCS A 24i 0720 24i 0845 HamzaSheikh asp
ASP process source files

assets
Image assets used by the game

Build

Install the required system packages first.

make

Run

After building, run the generated program files from the project folder.

arbiter.out
hip.out
asp.out

Docker

The Dockerfile installs build tools, SFML and extra packages from requirements.txt.

Notes

This repository includes the project source, assets, build files, report files, and supporting documents.
