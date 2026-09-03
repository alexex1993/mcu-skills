cmake_minimum_required(VERSION 3.16.0)

# Set before project(): otherwise ESP-IDF derives the version with `git
# describe`, which fails outright in a repository that has no commits yet --
# the usual state of a project one `git init` old. The build then stops during
# configuration with `fatal: not a git repository`, in a directory that is one.
set(PROJECT_VER "1.0.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32c6-touch-lcd-1_47)
