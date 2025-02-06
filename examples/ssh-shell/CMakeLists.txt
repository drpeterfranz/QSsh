add_executable(ssh-shell
    argumentscollector.cpp  main.cpp  shell.cpp)

target_link_libraries(ssh-shell PRIVATE QSsh Qt6::Core Qt6::Network botan::botan)

