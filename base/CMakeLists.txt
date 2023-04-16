file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h")
file(GLOB BASE_HEADERS "*.hpp" "*.h")

add_library(base STATIC ${BASE_SRC})
target_include_directories(base PRIVATE ${TINYGLTF_INCLUDE_DIRS} ${COMM_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(base ${OTH_LIBRARY} glm::glm imgui::imgui Vulkan::Vulkan KTX::ktx)


set_target_properties(base PROPERTIES 
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR};${COMM_INCLUDE_DIR}")
