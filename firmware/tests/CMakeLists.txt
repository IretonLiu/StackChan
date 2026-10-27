cmake_minimum_required(VERSION 3.16)

project(stackchan_firmware_tests LANGUAGES CXX)

enable_testing()

add_executable(motion_math_test
    motion_math_test.cpp
    ../main/stackchan/motion/motion_math.cpp
)

target_include_directories(motion_math_test PRIVATE
    ../main
)

target_compile_features(motion_math_test PRIVATE cxx_std_17)

add_test(NAME motion_math_test COMMAND $<TARGET_FILE:motion_math_test>)

add_executable(pet_hardware_policy_test
    pet_hardware_policy_test.cpp
    ../main/hal/pet/pet_hardware_policy.cpp
)

target_include_directories(pet_hardware_policy_test PRIVATE
    ../main
)

target_compile_features(pet_hardware_policy_test PRIVATE cxx_std_17)

add_test(NAME pet_hardware_policy_test COMMAND $<TARGET_FILE:pet_hardware_policy_test>)

add_executable(face_expression_test
    face_expression_test.cpp
    ../main/pet/face/face.cpp
    ../main/pet/face/glyph_paths.cpp
    ../main/pet/face/face_animation.cpp
)

target_include_directories(face_expression_test PRIVATE
    ../main
)

target_compile_features(face_expression_test PRIVATE cxx_std_17)

add_test(NAME face_expression_test COMMAND $<TARGET_FILE:face_expression_test>)

add_executable(shape_core_test
    shape_core_test.cpp
    ../main/pet/shape/circle.cpp
    ../main/pet/shape/rounded_polygon.cpp
    ../main/pet/shape/shape_path.cpp
    ../main/pet/shape/shape_face_adapter.cpp
    ../main/pet/shape/soft_polygon.cpp
)

target_include_directories(shape_core_test PRIVATE
    ../main
)

target_compile_features(shape_core_test PRIVATE cxx_std_17)

add_test(NAME shape_core_test COMMAND $<TARGET_FILE:shape_core_test>)

add_executable(face_preview
    face_preview.cpp
    face_svg_renderer.cpp
    ../main/pet/face/face.cpp
    ../main/pet/face/glyph_paths.cpp
)

target_include_directories(face_preview PRIVATE
    ../main
)

target_compile_features(face_preview PRIVATE cxx_std_17)

add_executable(shape_preview
    shape_preview.cpp
    ../main/pet/shape/shape_path.cpp
    ../main/pet/shape/rounded_polygon.cpp
    ../main/pet/shape/soft_polygon.cpp
)

target_include_directories(shape_preview PRIVATE
    ../main
)

target_compile_features(shape_preview PRIVATE cxx_std_17)

option(STACKCHAN_BUILD_SHAPE_STUDIO "Build the GLFW/Dear ImGui shape editor" OFF)

if(STACKCHAN_BUILD_SHAPE_STUDIO)
    set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tools/vendor/imgui")
    if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
        message(FATAL_ERROR "Shape Studio needs the imgui submodule. Run: git submodule update --init --recursive")
    endif()

    find_package(glfw3 3.3 REQUIRED)
    find_package(OpenGL REQUIRED)

    add_executable(shape_studio
        shape_studio.cpp
        ../main/pet/shape/shape_path.cpp
        ../main/pet/shape/rounded_polygon.cpp
        ../main/pet/shape/soft_polygon.cpp
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )

    target_include_directories(shape_studio PRIVATE
        ../main
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )

    target_compile_features(shape_studio PRIVATE cxx_std_17)
    target_link_libraries(shape_studio PRIVATE glfw OpenGL::GL)
endif()
