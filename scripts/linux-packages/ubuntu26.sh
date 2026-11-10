#!/usr/bin/env bash
#
# System packages required to build and run InSearchCreator on Ubuntu 26.04 (Resolute Raccoon).
#
# Third-party libraries (glfw, glew, glm, args, imgui, stb, nativefiledialog-extended,
# xlnt, googletest) are downloaded and built from source via CMake FetchContent;
# the packages below provide the compilers, build tools and system headers they need.
#
#   Toolchain:        build-essential cmake ninja-build git pkg-config
#   OpenGL:           libgl-dev                  (find_package(OpenGL) in Engine)
#                     libglu1-mesa-dev           (GL/glu.h, included by glew.h)
#   GLFW on X11:      libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev
#                     libxi-dev libxfixes-dev libxext-dev
#   GLFW on Wayland:  libwayland-dev wayland-protocols libxkbcommon-dev
#   nativefiledialog: libgtk-3-dev               (GTK3 backend of nfd)
#
# Runtime libraries (libgl1, libgtk-3-0t64, X11/Wayland libs) are pulled in
# automatically as dependencies of the -dev packages above.
#
# Usage: ./scripts/linux-packages/ubuntu26.sh   (asks for sudo password)

set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    libgl-dev \
    libglu1-mesa-dev \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libxfixes-dev \
    libxext-dev \
    libxkbcommon-dev \
    libwayland-dev \
    wayland-protocols \
    libgtk-3-dev
