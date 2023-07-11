CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
IF(COMMAND CMAKE_POLICY)
	CMAKE_POLICY(SET CMP0003 OLD)
ENDIF(COMMAND CMAKE_POLICY)

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
	
PROJECT(xtoysestimcontroller)

SET(CONTROLLER_SRC
src/estimsignaloutput.cpp
src/fileio.cpp
src/httpserver.cpp
src/main.cpp
src/miscfunctions.cpp
src/mongoose.c
src/udpsignalhandler.cpp
)

ADD_EXECUTABLE(xtoysestimcontroller ${CONTROLLER_SRC})

FIND_LIBRARY(PORTAUDIO_LIBRARY NAMES libportaudio portaudio)
IF(PORTAUDIO_LIBRARY)
	TARGET_LINK_LIBRARIES(xtoysestimcontroller ${PORTAUDIO_LIBRARY})
ELSE(PORTAUDIO_LIBRARY)
	MESSAGE(FATAL_ERROR "Could not find libgd.")
ENDIF(PORTAUDIO_LIBRARY)
