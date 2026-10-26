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
)

target_include_directories(face_expression_test PRIVATE
    ../main
)

target_compile_features(face_expression_test PRIVATE cxx_std_17)

add_test(NAME face_expression_test COMMAND $<TARGET_FILE:face_expression_test>)

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
