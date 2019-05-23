cmake_minimum_required(VERSION 3.10)

project(imguizmo)

find_package(imgui REQUIRED)

set(imguizmo_public_hdrs
    ${CMAKE_CURRENT_LIST_DIR}/ImCurveEdit.h
    ${CMAKE_CURRENT_LIST_DIR}/ImGradient.h
    ${CMAKE_CURRENT_LIST_DIR}/ImGuizmo.h
    ${CMAKE_CURRENT_LIST_DIR}/ImSequencer.h
)

add_library(${PROJECT_NAME}
    ${imguizmo_public_hdrs}
    ${CMAKE_CURRENT_LIST_DIR}/ImCurveEdit.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ImGradient.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ImGuizmo.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ImSequencer.cpp
)

add_library(::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:include/imguizmo>
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC ::imgui
)

add_subdirectory(example)

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX d)

configure_file(${PROJECT_NAME}-config.cmake.in ${PROJECT_NAME}-config.cmake @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}-config.cmake DESTINATION lib/cmake/${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}-targets DESTINATION lib)
install(FILES ${imguizmo_public_hdrs} DESTINATION include/${PROJECT_NAME})
install(EXPORT ${PROJECT_NAME}-targets NAMESPACE :: DESTINATION lib/cmake/${PROJECT_NAME})
