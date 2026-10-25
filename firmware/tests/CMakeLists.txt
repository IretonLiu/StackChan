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
