cmake_minimum_required(VERSION 3.16)

project(kraken_ws LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)

configure_file(
  "${PROJECT_SOURCE_DIR}/include/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
 )

#set(CMAKE_PREFIX_PATH "${PROJECT_SOURCE_DIR}/../../installs/qt-everywhere-src-6.5.1/qtbase/lib/cmake")
#set(Qt6_DIR "${PROJECT_SOURCE_DIR}/../../installs/qt-everywhere-src-6.5.1/qtbase/lib/cmake/Qt6")
#set(Qt_version 6)


set(CMAKE_PREFIX_PATH "${PROJECT_SOURCE_DIR}/../../installs/IXWebSocket/ixwebsocket/") #to find ixwebsocket


# generate list of source files
file(GLOB_RECURSE CODE_FILES
        "${PROJECT_SOURCE_DIR}/include/*.h"
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
    )



set(EXECUTABLE_NAME "kraken_ws")
add_executable(${EXECUTABLE_NAME} ${CODE_FILES})

#SET(Boost_USE_STATIC_LIBS OFF)
#SET(Boost_USE_MULTITHREAD ON)
#FIND_PACKAGE(Boost REQUIRED COMPONENTS system thread regex iostreams)#  1.66.0

#IF(Boost_FOUND)
    #INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
    #LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
#ENDIF(Boost_FOUND)


#FIND_LIBRARY(pthread_LIB pthread REQUIRED)
#FIND_LIBRARY(xml2_LIB xml2 REQUIRED)
FIND_LIBRARY(ixwebsocket_LIB PRIVATE libixwebsocket.a REQUIRED)
find_package(OpenSSL 1.1 REQUIRED)
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})
FIND_PACKAGE(ZLIB REQUIRED)




target_include_directories(${EXECUTABLE_NAME} PUBLIC "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/../../installs/rapidjson/include/" "${PROJECT_SOURCE_DIR}/../../installs/IXWebSocket/ixwebsocket/")

target_link_libraries(${EXECUTABLE_NAME} PRIVATE  "${ixwebsocket_LIB}" OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ) #"${pthread_LIB}" "${xml2_LIB}"

install(TARGETS kraken_ws RUNTIME DESTINATION bin)

