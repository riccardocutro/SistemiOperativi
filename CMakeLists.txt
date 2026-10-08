cmake_minimum_required(VERSION 3.20)
project(ProgettoSO_mensa C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wvla -Wextra -Werror -D_GNU_SOURCE")

set(SOURCE_COMMON common.c common.h)


add_executable(responsabile_mensa responsabile_mensa.c ${SOURCE_COMMON})

add_executable(operatore operatore.c ${SOURCE_COMMON})

add_executable(operatore_cassa operatore_cassa.c ${SOURCE_COMMON})

add_executable(utente utente.c ${SOURCE_COMMON})