cmake_minimum_required(VERSION 3.16)

project(stackchan_firmware_tests LANGUAGES CXX)

enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# These tests assert their results. A Release configuration defines NDEBUG, which
# compiles every assert() away and turns the suite into a no-op that always passes.
# Undefine it after CMAKE_CXX_FLAGS so assertions stay active in every build type.
add_compile_options(-UNDEBUG)

add_test(NAME face_preset_compiler_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/face_preset_compiler_test.py)

add_test(NAME face_schema_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/face_schema_test.py)

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

add_executable(motion_response_test
    motion_response_test.cpp
    ../main/pet/preset/motion_response.cpp
)

target_include_directories(motion_response_test PRIVATE
    ../main
)

target_compile_features(motion_response_test PRIVATE cxx_std_17)

add_test(NAME motion_response_test COMMAND $<TARGET_FILE:motion_response_test>)

add_executable(face_pose_engine_test
    face_pose_engine_test.cpp
    ../main/pet/preset/face_pose_engine.cpp
    ../main/pet/preset/motion_response.cpp
    ../main/pet/shape/rounded_polygon.cpp
    ../main/pet/shape/shape_path.cpp
)

target_include_directories(face_pose_engine_test PRIVATE
    ../main
)

target_compile_features(face_pose_engine_test PRIVATE cxx_std_17)

add_test(NAME face_pose_engine_test COMMAND $<TARGET_FILE:face_pose_engine_test>)

add_executable(face_animation_engine_test
    face_animation_engine_test.cpp
    ../main/pet/preset/face_animation_engine.cpp
    ../main/pet/preset/face_pose_engine.cpp
    ../main/pet/preset/motion_response.cpp
    ../main/pet/shape/rounded_polygon.cpp
    ../main/pet/shape/shape_path.cpp
)

target_include_directories(face_animation_engine_test PRIVATE
    ../main
)

target_compile_features(face_animation_engine_test PRIVATE cxx_std_17)

add_test(NAME face_animation_engine_test COMMAND $<TARGET_FILE:face_animation_engine_test>)

add_executable(shape_core_test
    shape_core_test.cpp
    ../main/pet/shape/circle.cpp
    ../main/pet/shape/rounded_polygon.cpp
    ../main/pet/shape/shape_path.cpp
    ../main/pet/shape/shape_face_adapter.cpp
    ../main/pet/preset/face_preset_evaluator.cpp
    ../main/pet/preset/face_preset_renderer.cpp
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

