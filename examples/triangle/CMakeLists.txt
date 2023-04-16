file(GLOB SOURCE_FILES "*.cpp")

add_executable(triangle ${SOURCE_FILES})
target_link_libraries(triangle base)
