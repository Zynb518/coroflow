cmake_minimum_required(VERSION 3.28)
project(coroflow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(enable_strict_warnings target_name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(coroflow INTERFACE)

target_sources(coroflow
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
        include/coroflow/TcpServer.hpp

        include/coroflow/TcpAcceptor.hpp
        include/coroflow/TcpSocket.hpp
        include/coroflow/SocketAddress.hpp

        include/coroflow/IoLoopSelector.hpp

        include/coroflow/TcpSession.hpp

        include/coroflow/EventLoopThreadPool.hpp
        include/coroflow/EventLoop.hpp
        include/coroflow/Epoller.hpp
        include/coroflow/FdContext.hpp
        include/coroflow/TcpConnection.hpp
        include/coroflow/EventNotifier.hpp
        include/coroflow/TimerQueue.hpp

        include/coroflow/Task.hpp
        include/coroflow/FdAwaiter.hpp

        include/coroflow/Result.hpp

        include/coroflow/Logger.hpp


)

target_include_directories(coroflow
    INTERFACE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(coroflow INTERFACE cxx_std_20)

add_executable(coroflow_dev
    main.cpp
)

target_link_libraries(coroflow_dev
    PRIVATE
        coroflow
)

target_compile_features(coroflow_dev PRIVATE cxx_std_20)
enable_strict_warnings(coroflow_dev)
