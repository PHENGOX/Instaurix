cmake_minimum_required(VERSION 3.20)
project(Instaurix VERSION 0.86 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(emu_core src/cpu.cpp src/cpu_pair.cpp src/cpu_integer.cpp src/cpu_load_store.cpp src/cpu_atomic.cpp src/cpu_fp.cpp src/cpu_softfloat.cpp src/cpu_fp_extended.cpp src/cpu_simd_integer.cpp src/cpu_simd_arithmetic.cpp src/cpu_simd_memory.cpp src/memory.cpp src/bus.cpp src/program_loader.cpp src/elf_loader.cpp src/test_device.cpp src/interrupt_controller.cpp src/timer.cpp src/machine.cpp src/process.cpp)
target_include_directories(emu_core PUBLIC src)
target_sources(emu_core PRIVATE src/ipc.cpp)
target_sources(emu_core PRIVATE src/ipc_nv.cpp)
target_sources(emu_core PRIVATE src/gpu_va.cpp)
target_sources(emu_core PRIVATE src/gpu_submission.cpp)
target_sources(emu_core PRIVATE src/gpu_packets.cpp)
target_sources(emu_core PRIVATE src/gpu_reference.cpp)
target_sources(emu_core PRIVATE src/gpu_classes.cpp src/gpu_channel.cpp src/gpu_maxwell_3d.cpp src/gpu_maxwell_copy.cpp)
target_sources(emu_core PRIVATE src/ipc_binder.cpp)
target_sources(emu_core PRIVATE src/ipc_time.cpp)
target_sources(emu_core PRIVATE src/ipc_domain.cpp)
target_sources(emu_core PRIVATE src/ipc_files.cpp src/read_only_files.cpp)
target_sources(emu_core PRIVATE src/frame.cpp src/ipc_display.cpp)
target_sources(emu_core PRIVATE src/native_window.cpp)
target_sources(emu_core PRIVATE src/nro_loader.cpp)
target_sources(emu_core PRIVATE src/nro_runtime.cpp)
if(WIN32)
    target_link_libraries(emu_core PUBLIC user32 gdi32)
endif()
add_executable(Instaurix src/main.cpp)
target_link_libraries(Instaurix PRIVATE emu_core)
include(CTest)
if(BUILD_TESTING)
    if(WIN32)
        add_executable(host_test_runner tools/host_test_runner.cpp)
        target_link_libraries(host_test_runner PRIVATE psapi)
        add_executable(host_diagnostic_probe tests/host_diagnostic_probe.cpp)
        add_test(NAME windows_host_diagnostic_capture
            COMMAND ${CMAKE_COMMAND}
                "-DRUNNER=$<TARGET_FILE:host_test_runner>"
                "-DPROBE=$<TARGET_FILE:host_diagnostic_probe>"
                "-DCTEST=${CMAKE_CTEST_COMMAND}"
                "-DREPORTS=${CMAKE_CURRENT_BINARY_DIR}/diagnostics/$<CONFIG>/self-test"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/host_diagnostics.cmake")
    endif()
    function(add_emu_test)
        if(WIN32)
            cmake_parse_arguments(TEST "EXPECT_FAILURE" "NAME" "COMMAND" ${ARGN})
            list(POP_FRONT TEST_COMMAND executable)
            add_test(NAME ${TEST_NAME} COMMAND host_test_runner
                "${CMAKE_CURRENT_BINARY_DIR}/diagnostics/$<CONFIG>" "${TEST_NAME}"
                "$<IF:$<BOOL:${TEST_EXPECT_FAILURE}>,--expect-failure,--expect-success>"
                "$<TARGET_FILE:${executable}>" ${TEST_COMMAND})
            set_tests_properties(${TEST_NAME} PROPERTIES FAIL_REGULAR_EXPRESSION "HOST_DIAGNOSTIC_FAILURE:")
        else()
            cmake_parse_arguments(TEST "EXPECT_FAILURE" "NAME" "COMMAND" ${ARGN})
            add_test(NAME ${TEST_NAME} COMMAND ${TEST_COMMAND})
            if(TEST_EXPECT_FAILURE)
                set_tests_properties(${TEST_NAME} PROPERTIES WILL_FAIL TRUE)
            endif()
        endif()
    endfunction()
    add_executable(transfer_memory_tests tests/transfer_memory_tests.cpp)
    target_link_libraries(transfer_memory_tests PRIVATE emu_core)
    add_emu_test(NAME transfer_memory_ownership_and_lifetime COMMAND transfer_memory_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_window/program.nro")
    add_executable(libnx_nv_tests tests/libnx_nv_tests.cpp)
    target_link_libraries(libnx_nv_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_nvmap_create_free_cycle COMMAND libnx_nv_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_nvmap_cycle/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_nvmap_cycle/symbols.txt" --cycle)
    add_emu_test(NAME libnx_original_nvmap_wrapper COMMAND libnx_nv_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_nvmap_wrapper/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_nvmap_wrapper/symbols.txt" --wrapper)
    add_executable(libnx_fence_tests tests/libnx_fence_tests.cpp)
    target_link_libraries(libnx_fence_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_fence_device_lifetime COMMAND libnx_fence_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_fence/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_fence/symbols.txt")
    add_emu_test(NAME libnx_original_nvmap_create_boundary COMMAND libnx_nv_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_nvmap_alloc/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_nvmap_alloc/symbols.txt" --alloc)
    add_emu_test(NAME libnx_original_nvmap_device_boundary COMMAND libnx_nv_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_nvmap/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_nvmap/symbols.txt" --nvmap)
    add_emu_test(NAME libnx_original_nv_transfer_memory COMMAND libnx_nv_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_nv/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_nv/symbols.txt")
    add_executable(libnx_window_tests tests/libnx_window_tests.cpp)
    target_link_libraries(libnx_window_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_native_window_event COMMAND libnx_window_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_window/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_window/symbols.txt")
    add_emu_test(NAME libnx_original_buffer_registration_boundary COMMAND libnx_window_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_register/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_register/symbols.txt" --register)
    add_emu_test(NAME libnx_original_buffer_dequeue_cancel COMMAND libnx_window_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_dequeue/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_dequeue/symbols.txt" --dequeue)
    add_emu_test(NAME libnx_original_cpu_buffer_presentation COMMAND libnx_window_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_present/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_present/symbols.txt" --present)
    add_executable(libnx_framebuffer_tests tests/libnx_framebuffer_tests.cpp)
    add_executable(libnx_gpu_as_tests tests/libnx_gpu_as_tests.cpp)
    add_executable(libnx_gpu_va_tests tests/libnx_gpu_va_tests.cpp)
    add_executable(libnx_gpu_map_tests tests/libnx_gpu_map_tests.cpp)
    add_executable(libnx_gpu_channel_tests tests/libnx_gpu_channel_tests.cpp)
    add_executable(libnx_gpu_bind_tests tests/libnx_gpu_bind_tests.cpp)
    add_executable(libnx_gpu_fifo_tests tests/libnx_gpu_fifo_tests.cpp)
    add_executable(libnx_gpu_context_tests tests/libnx_gpu_context_tests.cpp)
    add_executable(libnx_gpu_event_tests tests/libnx_gpu_event_tests.cpp)
    add_executable(libnx_gpu_notifier_tests tests/libnx_gpu_notifier_tests.cpp)
    add_executable(libnx_gpu_channel_init_tests tests/libnx_gpu_channel_init_tests.cpp)
    add_executable(libnx_gpu_ioctl2_tests tests/libnx_gpu_ioctl2_tests.cpp)
    add_executable(gpu_submission_tests tests/gpu_submission_tests.cpp)
    add_executable(gpu_packets_tests tests/gpu_packets_tests.cpp)
    add_executable(gpu_reference_tests tests/gpu_reference_tests.cpp)
    add_executable(gpu_channel_tests tests/gpu_channel_tests.cpp)
    add_executable(gpu_copy_tests tests/gpu_copy_tests.cpp)
    target_link_libraries(gpu_copy_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_maxwell_linear_copy_plan COMMAND gpu_copy_tests)
    add_executable(gpu_profile_tests tests/gpu_profile_tests.cpp)
    target_link_libraries(gpu_profile_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_selected_profile_lifecycle COMMAND gpu_profile_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/symbols.txt")
    set(gpu_profile_runner_args)
    if(WIN32)
        list(APPEND gpu_profile_runner_args "-DRUNNER=$<TARGET_FILE:host_test_runner>" "-DREPORTS=${CMAKE_CURRENT_BINARY_DIR}/diagnostics/$<CONFIG>/cli")
    endif()
    add_test(NAME gpu_profile_cli_contract COMMAND ${CMAKE_COMMAND}
        "-DAPP=$<TARGET_FILE:Instaurix>" "-DPROGRAM=${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/program.nro"
        ${gpu_profile_runner_args} -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpu_profile_cli.cmake")
    target_link_libraries(gpu_channel_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_profiles_classes_and_subchannels COMMAND gpu_channel_tests)
    add_executable(gpu_memory_release_tests tests/gpu_memory_release_tests.cpp)
    target_link_libraries(gpu_memory_release_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_four_byte_release_plan COMMAND gpu_memory_release_tests)
    target_link_libraries(gpu_reference_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_local_reference_execution COMMAND gpu_reference_tests)
    add_executable(libnx_gpu_copy_guest_tests tests/libnx_gpu_copy_guest_tests.cpp)
    add_executable(libnx_gpu_copy_2d_guest_tests tests/libnx_gpu_copy_2d_guest_tests.cpp)
    add_executable(libnx_gpu_copy_block_guest_tests tests/libnx_gpu_copy_block_guest_tests.cpp)
    target_link_libraries(libnx_gpu_copy_block_guest_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_guest_generated_block_conversion COMMAND libnx_gpu_copy_block_guest_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy_block/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy_block/symbols.txt")
    add_emu_test(NAME libnx_guest_block_detects_missing_conversion COMMAND libnx_gpu_copy_block_guest_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy_block/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy_block/symbols.txt" --suppress-copy)
    add_emu_test(NAME libnx_guest_block_detects_padding_corruption COMMAND libnx_gpu_copy_block_guest_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy_block/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy_block/symbols.txt" --corrupt-padding)
    target_link_libraries(libnx_gpu_copy_2d_guest_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_guest_generated_2d_copy COMMAND libnx_gpu_copy_2d_guest_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy_2d/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy_2d/symbols.txt")
    add_emu_test(NAME libnx_guest_2d_detects_missing_transfer COMMAND libnx_gpu_copy_2d_guest_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy_2d/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy_2d/symbols.txt" --suppress-copy)
    add_emu_test(NAME libnx_guest_2d_detects_padding_corruption COMMAND libnx_gpu_copy_2d_guest_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy_2d/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy_2d/symbols.txt" --corrupt-padding)
    target_link_libraries(libnx_gpu_copy_guest_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_guest_generated_copy_and_verification COMMAND libnx_gpu_copy_guest_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy/symbols.txt")
    add_emu_test(NAME libnx_guest_copy_detects_missing_transfer COMMAND libnx_gpu_copy_guest_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_copy/symbols.txt" --suppress-copy)
    add_executable(libnx_gpu_packets_tests tests/libnx_gpu_packets_tests.cpp)
    add_executable(libnx_gpu_execution_failure_tests tests/libnx_gpu_packets_tests.cpp)
    add_executable(libnx_gpu_modular_tests tests/libnx_gpu_packets_tests.cpp)
    add_executable(libnx_gpu_copy_tests tests/libnx_gpu_packets_tests.cpp)
    target_compile_definitions(libnx_gpu_copy_tests PRIVATE GPU_LINEAR_COPY_TESTS GPU_ALLOCATION_FAILURE_TESTS)
    target_link_libraries(libnx_gpu_copy_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_mapped_linear_copy_transaction COMMAND libnx_gpu_copy_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/symbols.txt")
    target_compile_definitions(libnx_gpu_modular_tests PRIVATE GPU_MODULAR_CHANNEL_TESTS GPU_ALLOCATION_FAILURE_TESTS)
    target_link_libraries(libnx_gpu_modular_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_class_binding_reply_transaction COMMAND libnx_gpu_modular_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/symbols.txt")
    add_executable(libnx_gpu_memory_release_tests tests/libnx_gpu_packets_tests.cpp)
    target_compile_definitions(libnx_gpu_memory_release_tests PRIVATE GPU_MEMORY_RELEASE_TESTS GPU_ALLOCATION_FAILURE_TESTS)
    target_link_libraries(libnx_gpu_memory_release_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_mapped_release_transaction COMMAND libnx_gpu_memory_release_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/symbols.txt")
    target_compile_definitions(libnx_gpu_execution_failure_tests PRIVATE GPU_ALLOCATION_FAILURE_TESTS)
    target_link_libraries(libnx_gpu_execution_failure_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_submission_allocation_rollback COMMAND libnx_gpu_execution_failure_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/symbols.txt")
    target_link_libraries(libnx_gpu_packets_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_packet_decoding COMMAND libnx_gpu_packets_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_packets/symbols.txt")
    target_link_libraries(gpu_packets_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_historical_packet_decoder COMMAND gpu_packets_tests)
    add_executable(libnx_gpu_preflight_tests tests/libnx_gpu_preflight_tests.cpp)
    target_link_libraries(libnx_gpu_preflight_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_mapped_preflight COMMAND libnx_gpu_preflight_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_preflight/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_preflight/symbols.txt")
    target_link_libraries(gpu_submission_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_submission_preflight COMMAND gpu_submission_tests)
    add_executable(nv_ioctl2_tests tests/nv_ioctl2_tests.cpp)
    target_link_libraries(nv_ioctl2_tests PRIVATE emu_core)
    add_emu_test(NAME nv_ioctl2_transport_validation COMMAND nv_ioctl2_tests)
    target_link_libraries(libnx_gpu_ioctl2_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_ioctl2_transport COMMAND libnx_gpu_ioctl2_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_ioctl2/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_ioctl2/symbols.txt")
    target_link_libraries(libnx_gpu_channel_init_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_channel_initialization COMMAND libnx_gpu_channel_init_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_channel_init/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_channel_init/symbols.txt")
    target_link_libraries(libnx_gpu_notifier_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_error_notifier COMMAND libnx_gpu_notifier_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_notifier/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_notifier/symbols.txt")
    target_link_libraries(libnx_gpu_event_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_error_event COMMAND libnx_gpu_event_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_event/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_event/symbols.txt")
    target_link_libraries(libnx_gpu_context_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_graphics_context COMMAND libnx_gpu_context_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_context/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_context/symbols.txt")
    target_link_libraries(libnx_gpu_fifo_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_fifo_allocation COMMAND libnx_gpu_fifo_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_fifo/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_fifo/symbols.txt")
    target_link_libraries(libnx_gpu_bind_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_address_space_binding COMMAND libnx_gpu_bind_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_bind/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_bind/symbols.txt")
    target_link_libraries(libnx_gpu_channel_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_channel_lifecycle COMMAND libnx_gpu_channel_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_channel/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_channel/symbols.txt")
    target_link_libraries(libnx_gpu_map_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_backing_mappings COMMAND libnx_gpu_map_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_map/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_map/symbols.txt")
    target_link_libraries(libnx_gpu_va_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_virtual_allocations COMMAND libnx_gpu_va_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_va/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_va/symbols.txt")
    add_executable(gpu_va_tests tests/gpu_va_tests.cpp)
    add_executable(gpu_mapping_tests tests/gpu_mapping_tests.cpp)
    target_link_libraries(gpu_mapping_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_virtual_backing_mappings COMMAND gpu_mapping_tests)
    target_link_libraries(gpu_va_tests PRIVATE emu_core)
    add_emu_test(NAME gpu_virtual_address_reservations COMMAND gpu_va_tests)
    target_link_libraries(libnx_gpu_as_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_gpu_address_space_lifecycle COMMAND libnx_gpu_as_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_as/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_gpu_as/symbols.txt")
    add_executable(libnx_framebuffer_buffers_tests tests/libnx_framebuffer_buffers_tests.cpp)
    target_link_libraries(libnx_framebuffer_buffers_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_framebuffer_buffer_rotation COMMAND libnx_framebuffer_buffers_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_framebuffer_buffers/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_framebuffer_buffers/symbols.txt")
    add_executable(cpu_cache_tests tests/cpu_cache_tests.cpp)
    target_link_libraries(cpu_cache_tests PRIVATE emu_core)
    add_emu_test(NAME cpu_local_cache_maintenance COMMAND cpu_cache_tests)
    target_link_libraries(libnx_framebuffer_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_framebuffer_create_boundary COMMAND libnx_framebuffer_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_framebuffer/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_framebuffer/symbols.txt")
    add_emu_test(NAME libnx_original_framebuffer_presentation COMMAND libnx_framebuffer_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_framebuffer_present/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_framebuffer_present/symbols.txt" --present)
    add_emu_test(NAME libnx_original_framebuffer_padded_geometry COMMAND libnx_framebuffer_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_framebuffer_wide/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_framebuffer_wide/symbols.txt" --wide)
    add_executable(libnx_queue_tests tests/libnx_queue_tests.cpp)
    target_link_libraries(libnx_queue_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_queue_connection COMMAND libnx_queue_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_queue/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_queue/symbols.txt")
    add_executable(libnx_vi_tests tests/libnx_vi_tests.cpp)
    target_link_libraries(libnx_vi_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_binder_lifetime COMMAND libnx_vi_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_binder/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_binder/symbols.txt" --binder)
    add_emu_test(NAME libnx_original_vi_sessions_and_display COMMAND libnx_vi_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_vi/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_vi/symbols.txt")
    add_executable(libnx_posix_tests tests/libnx_posix_tests.cpp)
    target_link_libraries(libnx_posix_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_guest_posix_open_read_close COMMAND libnx_posix_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_posix/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_posix/symbols.txt")
    add_executable(native_file_tests tests/native_file_tests.cpp)
    target_link_libraries(native_file_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_native_file_open_and_read COMMAND native_file_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_fsdev/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_fsdev/symbols.txt")
    add_executable(libnx_fsdev_tests tests/libnx_fsdev_tests.cpp)
    target_link_libraries(libnx_fsdev_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_fsdev_mount_and_main COMMAND libnx_fsdev_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_fsdev/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_fsdev/symbols.txt")
    add_executable(libnx_tls_tests tests/libnx_tls_tests.cpp)
    target_link_libraries(libnx_tls_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_tls_sections_and_contexts COMMAND libnx_tls_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro" "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/symbols.txt")
    add_executable(fsp_initialization_tests tests/fsp_initialization_tests.cpp)
    target_link_libraries(fsp_initialization_tests PRIVATE emu_core)
    add_emu_test(NAME original_fsp_process_attachment_and_clones COMMAND fsp_initialization_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/symbols.txt")
    add_executable(newlib_environment_tests tests/newlib_environment_tests.cpp)
    target_link_libraries(newlib_environment_tests PRIVATE emu_core)
    add_emu_test(NAME original_newlib_environment_and_allocator COMMAND newlib_environment_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/symbols.txt")
    add_executable(time_service_tests tests/time_service_tests.cpp)
    target_link_libraries(time_service_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_time_clocks_and_utc_calendar COMMAND time_service_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro")
    add_executable(hid_memory_tests tests/hid_memory_tests.cpp)
    target_link_libraries(hid_memory_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_hid_resources_and_shared_memory COMMAND hid_memory_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro")
    add_executable(applet_operation_tests tests/applet_operation_tests.cpp)
    target_link_libraries(applet_operation_tests PRIVATE emu_core)
    add_emu_test(NAME applet_operation_profile_and_notifications COMMAND applet_operation_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro")
    add_executable(applet_focus_tests tests/applet_focus_tests.cpp)
    target_link_libraries(applet_focus_tests PRIVATE emu_core)
    add_emu_test(NAME applet_focus_policy_and_suspension COMMAND applet_focus_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro")
    add_executable(applet_state_tests tests/applet_state_tests.cpp)
    target_link_libraries(applet_state_tests PRIVATE emu_core)
    add_emu_test(NAME applet_identity_messages_and_native_wait COMMAND applet_state_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro")
    add_executable(cpu_counter_tests tests/cpu_counter_tests.cpp)
    target_link_libraries(cpu_counter_tests PRIVATE emu_core)
    add_emu_test(NAME cpu_counter_and_virtual_clock COMMAND cpu_counter_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro")
    add_executable(ipc_domain_tests tests/ipc_domain_tests.cpp)
    target_link_libraries(ipc_domain_tests PRIVATE emu_core)
    add_emu_test(NAME native_applet_domains_and_lifetimes COMMAND ipc_domain_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro")
    add_executable(libnx_app_tests tests/libnx_app_tests.cpp)
    target_link_libraries(libnx_app_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_default_app_and_native_apm COMMAND libnx_app_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro" "${CMAKE_CURRENT_BINARY_DIR}")
    add_executable(libnx_sm_tests tests/libnx_sm_tests.cpp)
    target_link_libraries(libnx_sm_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_sm_ipc COMMAND libnx_sm_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_sm/program.nro" "${CMAKE_CURRENT_BINARY_DIR}")
    add_executable(libnx_init_tests tests/libnx_init_tests.cpp)
    target_link_libraries(libnx_init_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_init_to_main COMMAND libnx_init_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_init/program.nro" "${CMAKE_CURRENT_BINARY_DIR}")
    add_executable(cpu_movi_tests tests/cpu_movi_tests.cpp)
    target_link_libraries(cpu_movi_tests PRIVATE emu_core)
    add_emu_test(NAME cpu_movi_mask_independent_reference COMMAND cpu_movi_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/movi_reference.bin")
    add_executable(libnx_crt0_tests tests/libnx_crt0_tests.cpp)
    target_link_libraries(libnx_crt0_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_original_crt0_and_relro COMMAND libnx_crt0_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_crt0/program.nro" "${CMAKE_CURRENT_BINARY_DIR}")
    add_executable(nro_bootstrap_tests tests/nro_bootstrap_tests.cpp)
    target_link_libraries(nro_bootstrap_tests PRIVATE emu_core)
    add_emu_test(NAME nro_homebrew_entry_and_validation COMMAND nro_bootstrap_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/nro_bootstrap/program.nro" "${CMAKE_CURRENT_BINARY_DIR}")
    add_executable(libnx_dynamic_tests tests/libnx_dynamic_tests.cpp)
    target_link_libraries(libnx_dynamic_tests PRIVATE emu_core)
    add_emu_test(NAME libnx_upstream_dynamic_runtime COMMAND libnx_dynamic_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_dynamic/program.elf")
    add_executable(nro_loader_tests tests/nro_loader_tests.cpp)
    target_link_libraries(nro_loader_tests PRIVATE emu_core)
    add_emu_test(NAME nro_loader_and_runtime_boundary COMMAND nro_loader_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/nro_client/program.nro"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/nro_client/program.elf" "${CMAKE_CURRENT_BINARY_DIR}")
    add_emu_test(NAME nro_compiled_client_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/nro_client/program.nro" 50000 --hle-reference)
    set_tests_properties(nro_compiled_client_cli PROPERTIES PASS_REGULAR_EXPRESSION "Normal exit.*code=0")
    add_executable(hle_input_tests tests/hle_input_tests.cpp)
    target_link_libraries(hle_input_tests PRIVATE emu_core)
    add_emu_test(NAME hle_interactive_input COMMAND hle_input_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_interactive/program.elf"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/root")
    add_executable(hle_animation_tests tests/hle_animation_tests.cpp)
    target_link_libraries(hle_animation_tests PRIVATE emu_core)
    add_emu_test(NAME hle_animation_stream COMMAND hle_animation_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_animation/program.elf"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/root")
    add_emu_test(NAME hle_animation_sequence_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_animation/program.elf" 10000000 --hle-reference
        --read-root "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/root"
        --frames-out "${CMAKE_CURRENT_BINARY_DIR}/animation-cli")
    set_tests_properties(hle_animation_sequence_cli PROPERTIES PASS_REGULAR_EXPRESSION "Frames exported=8 VirtualTimeNs=700000000")
    add_emu_test(NAME hle_window_requires_hle EXPECT_FAILURE COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/program.bin" --window)
    
    add_executable(hle_graphics_tests tests/hle_graphics_tests.cpp)
    target_link_libraries(hle_graphics_tests PRIVATE emu_core)
    add_emu_test(NAME hle_first_graphics COMMAND hle_graphics_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/program.elf"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/root" "${CMAKE_CURRENT_BINARY_DIR}")
    add_emu_test(NAME hle_first_graphics_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/program.elf" 1000000 --hle-reference
        --read-root "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/root"
        --frame-out "${CMAKE_CURRENT_BINARY_DIR}/graphics-cli.png")
    set_tests_properties(hle_first_graphics_cli PROPERTIES PASS_REGULAR_EXPRESSION "Frame exported.*160x96")
    add_emu_test(NAME hle_frame_export_requires_frame EXPECT_FAILURE COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/program.elf" 1 --hle-reference
        --frame-out "${CMAKE_CURRENT_BINARY_DIR}/no-frame.png")
    
    add_executable(hle_file_tests tests/hle_file_tests.cpp)
    target_link_libraries(hle_file_tests PRIVATE emu_core)
    add_emu_test(NAME hle_read_only_files COMMAND hle_file_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_files/program.elf"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_files/root" "${CMAKE_CURRENT_BINARY_DIR}")
    add_emu_test(NAME hle_read_only_files_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_files/program.elf" 50000 --hle-reference
        --read-root "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_files/root")
    set_tests_properties(hle_read_only_files_cli PROPERTIES PASS_REGULAR_EXPRESSION "Normal exit.*code=0")
    add_emu_test(NAME hle_read_root_requires_hle EXPECT_FAILURE COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_files/program.elf"
        --read-root "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_files/root")
    
    add_executable(hle_client_tests tests/hle_client_tests.cpp)
    target_link_libraries(hle_client_tests PRIVATE emu_core)
    add_emu_test(NAME hle_compiled_client_and_controls COMMAND hle_client_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_client/program.elf")
    add_emu_test(NAME hle_compiled_client_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_client/program.elf" 50000 --hle-reference)
    set_tests_properties(hle_compiled_client_cli PROPERTIES PASS_REGULAR_EXPRESSION "Normal exit.*code=0")
    add_executable(hle_sm_tests tests/hle_sm_tests.cpp)
    target_link_libraries(hle_sm_tests PRIVATE emu_core)
    add_emu_test(NAME hle_service_manager COMMAND hle_sm_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_sm/program.elf")
    add_emu_test(NAME hle_service_manager_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_sm/program.elf" 1000 --hle-reference)
    set_tests_properties(hle_service_manager_cli PROPERTIES PASS_REGULAR_EXPRESSION "Normal exit.*code=0")
    add_executable(hle_ipc_buffer_tests tests/hle_ipc_buffer_tests.cpp)
    target_link_libraries(hle_ipc_buffer_tests PRIVATE emu_core)
    add_emu_test(NAME hle_ipc_buffer_transfers COMMAND hle_ipc_buffer_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_ipc_buffers/program.elf")
    add_emu_test(NAME hle_ipc_buffers_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_ipc_buffers/program.elf" 10000 --hle-reference)
    set_tests_properties(hle_ipc_buffers_cli PROPERTIES PASS_REGULAR_EXPRESSION "Normal exit.*code=0")
    add_executable(hle_ipc_tests tests/hle_ipc_tests.cpp)
    target_link_libraries(hle_ipc_tests PRIVATE emu_core)
    add_emu_test(NAME hle_ipc_sessions_and_messages COMMAND hle_ipc_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_ipc/program.elf")
    add_emu_test(NAME hle_ipc_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_ipc/program.elf" 1000 --hle-reference)
    set_tests_properties(hle_ipc_cli PROPERTIES PASS_REGULAR_EXPRESSION "Normal exit.*code=0")
    add_executable(hle_time_tests tests/hle_time_tests.cpp)
    target_link_libraries(hle_time_tests PRIVATE emu_core)
    add_emu_test(NAME hle_virtual_time COMMAND hle_time_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_time/program.elf")
    add_emu_test(NAME hle_virtual_time_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_time/program.elf" 1000 --hle-reference)
    set_tests_properties(hle_virtual_time_cli PROPERTIES PASS_REGULAR_EXPRESSION "Normal exit.*code=0")
    add_executable(hle_thread_tests tests/hle_thread_tests.cpp)
    target_link_libraries(hle_thread_tests PRIVATE emu_core)
    add_emu_test(NAME hle_threads_scheduler COMMAND hle_thread_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_threads/program.elf")
    add_emu_test(NAME hle_threads_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_threads/program.elf" 2000 --hle-reference)
    set_tests_properties(hle_threads_cli PROPERTIES PASS_REGULAR_EXPRESSION "Normal exit.*code=0")
    add_executable(hle_event_tests tests/hle_event_tests.cpp)
    target_link_libraries(hle_event_tests PRIVATE emu_core)
    add_emu_test(NAME hle_events_suspend_resume COMMAND hle_event_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_events/program.elf")
    add_emu_test(NAME hle_events_waiting_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_events/program.elf" 100 --hle-reference)
    set_tests_properties(hle_events_waiting_cli PROPERTIES PASS_REGULAR_EXPRESSION "Waiting for a system event")
    add_executable(cpu_fp_tests tests/cpu_fp_tests.cpp)
    target_link_libraries(cpu_fp_tests PRIVATE emu_core)
    if(MSVC)
        target_compile_options(cpu_fp_tests PRIVATE /fp:strict)
    else()
        target_compile_options(cpu_fp_tests PRIVATE -frounding-math -ffp-contract=off)
    endif()
    add_emu_test(NAME cpu_fp_simd_independent_reference COMMAND cpu_fp_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_fp/reference.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/program.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_fp/reference-layout.txt")
    add_emu_test(NAME cpu_subnormal_independent_reference COMMAND cpu_fp_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_subnormal/reference.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/program.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_subnormal/reference-layout.txt")
    add_emu_test(NAME cpu_simd_arithmetic_independent_reference COMMAND cpu_fp_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_simd_arithmetic/reference.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/program.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_simd_arithmetic/reference-layout.txt")
    add_executable(cpu_fp_compiled_tests tests/cpu_fp_compiled_tests.cpp)
    target_link_libraries(cpu_fp_compiled_tests PRIVATE emu_core)
    if(MSVC)
        target_compile_options(cpu_fp_compiled_tests PRIVATE /fp:strict)
    else()
        target_compile_options(cpu_fp_compiled_tests PRIVATE -frounding-math -ffp-contract=off)
    endif()
    add_emu_test(NAME cpu_fp_compiled_native_comparison COMMAND cpu_fp_compiled_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_fp/program.elf")
    add_emu_test(NAME cpu_fp_workload_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_fp/program.elf" 1000 --test-protocol)
    set_tests_properties(cpu_fp_workload_cli PROPERTIES PASS_REGULAR_EXPRESSION "Normal exit.*code=0")
    add_executable(cpu_atomic_tests tests/cpu_atomic_tests.cpp)
    add_executable(memory_tracking_tests tests/memory_tracking_tests.cpp)
    target_link_libraries(memory_tracking_tests PRIVATE emu_core)
    add_emu_test(NAME sparse_memory_reservation_tracking COMMAND memory_tracking_tests)
    target_link_libraries(cpu_atomic_tests PRIVATE emu_core)
    add_emu_test(NAME cpu_atomic_and_exclusive COMMAND cpu_atomic_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/atomic_vectors.bin")
    add_executable(legacy_tests tests/legacy_tests.cpp)
    target_link_libraries(legacy_tests PRIVATE emu_core)
    add_emu_test(NAME legacy COMMAND legacy_tests)
    # Keep the large virtual-memory regression isolated from other test processes.
    set_tests_properties(legacy PROPERTIES RUN_SERIAL TRUE)
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE emu_core)
    add_emu_test(NAME core COMMAND core_tests)
    add_emu_test(NAME external_program COMMAND Instaurix "${CMAKE_CURRENT_SOURCE_DIR}/program.bin" 4)
    set_tests_properties(external_program PROPERTIES PASS_REGULAR_EXPRESSION "X2=12")
    add_emu_test(NAME missing_program EXPECT_FAILURE COMMAND Instaurix "${CMAKE_CURRENT_BINARY_DIR}/missing-program.bin")
    add_emu_test(NAME invalid_budget EXPECT_FAILURE COMMAND Instaurix "${CMAKE_CURRENT_SOURCE_DIR}/program.bin" -1)
    
    add_executable(cpu_pair_tests tests/cpu_pair_tests.cpp)
    target_link_libraries(cpu_pair_tests PRIVATE emu_core)
    add_emu_test(NAME cpu_pairs_and_compiled_program COMMAND cpu_pair_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/pair_vectors.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/stack_sum/program.bin")
    add_emu_test(NAME compiled_program_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/stack_sum/program.bin" 500 --test-protocol)
    set_tests_properties(compiled_program_cli PROPERTIES PASS_REGULAR_EXPRESSION "Normal exit.*code=0")
    add_emu_test(NAME compiled_program_protocol_disabled EXPECT_FAILURE COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/stack_sum/program.bin" 500)
    
    add_executable(elf_loader_tests tests/elf_loader_tests.cpp)
    target_link_libraries(elf_loader_tests PRIVATE emu_core)
    add_emu_test(NAME elf_loader_and_permissions COMMAND elf_loader_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/elf_sum/program.elf"
        "${CMAKE_CURRENT_SOURCE_DIR}/program.bin" "${CMAKE_CURRENT_BINARY_DIR}")
    add_emu_test(NAME elf_program_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/elf_sum/program.elf" 500 --test-protocol)
    set_tests_properties(elf_program_cli PROPERTIES PASS_REGULAR_EXPRESSION "X2=55")
    add_emu_test(NAME elf_program_protocol_disabled EXPECT_FAILURE COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/elf_sum/program.elf" 500)
    
    add_executable(hle_tests tests/hle_tests.cpp)
    target_link_libraries(hle_tests PRIVATE emu_core)
    add_emu_test(NAME hle_process_memory_and_tls COMMAND hle_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_vectors.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_memory/program.elf"
        "${CMAKE_CURRENT_SOURCE_DIR}/program.bin" "${CMAKE_CURRENT_BINARY_DIR}")
    add_emu_test(NAME hle_program_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_memory/program.elf" 500 --hle-reference)
    set_tests_properties(hle_program_cli PROPERTIES PASS_REGULAR_EXPRESSION "X2=55")
    add_emu_test(NAME hle_program_disabled EXPECT_FAILURE COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_memory/program.elf" 500)
    
    add_emu_test(NAME conflicting_runtime_options EXPECT_FAILURE COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_memory/program.elf" --test-protocol --hle-reference)
    
    add_executable(cpu_integer_tests tests/cpu_integer_tests.cpp)
    target_link_libraries(cpu_integer_tests PRIVATE emu_core)
    add_emu_test(NAME cpu_integer_independent_reference COMMAND cpu_integer_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_integer/reference.bin")
    add_executable(cpu_compiled_tests tests/cpu_compiled_tests.cpp)
    target_link_libraries(cpu_compiled_tests PRIVATE emu_core)
    add_emu_test(NAME cpu_compiled_native_comparison COMMAND cpu_compiled_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_integer/program.elf")
    add_emu_test(NAME cpu_integer_workload_cli COMMAND Instaurix
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_integer/program.elf" 5000 --test-protocol)
    set_tests_properties(cpu_integer_workload_cli PROPERTIES PASS_REGULAR_EXPRESSION "Normal exit.*code=0")
endif()



