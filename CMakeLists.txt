cmake_minimum_required(VERSION 3.20)
project(Switch2Emu VERSION 0.86 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(emu_core src/cpu.cpp src/cpu_pair.cpp src/cpu_integer.cpp src/cpu_load_store.cpp src/cpu_atomic.cpp src/cpu_fp.cpp src/cpu_softfloat.cpp src/cpu_fp_extended.cpp src/cpu_simd_integer.cpp src/cpu_simd_arithmetic.cpp src/cpu_simd_memory.cpp src/memory.cpp src/bus.cpp src/program_loader.cpp src/elf_loader.cpp src/test_device.cpp src/interrupt_controller.cpp src/timer.cpp src/machine.cpp src/process.cpp)
target_include_directories(emu_core PUBLIC src)
target_sources(emu_core PRIVATE src/ipc.cpp)
target_sources(emu_core PRIVATE src/ipc_domain.cpp)
target_sources(emu_core PRIVATE src/ipc_files.cpp src/read_only_files.cpp)
target_sources(emu_core PRIVATE src/frame.cpp src/ipc_display.cpp)
target_sources(emu_core PRIVATE src/native_window.cpp)
target_sources(emu_core PRIVATE src/nro_loader.cpp)
target_sources(emu_core PRIVATE src/nro_runtime.cpp)
if(WIN32)
    target_link_libraries(emu_core PUBLIC user32 gdi32)
endif()
add_executable(Switch2Emu src/main.cpp)
target_link_libraries(Switch2Emu PRIVATE emu_core)
include(CTest)
if(BUILD_TESTING)
    add_executable(applet_state_tests tests/applet_state_tests.cpp)
    target_link_libraries(applet_state_tests PRIVATE emu_core)
    add_test(NAME applet_identity_messages_and_native_wait COMMAND applet_state_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro")
    add_executable(cpu_counter_tests tests/cpu_counter_tests.cpp)
    target_link_libraries(cpu_counter_tests PRIVATE emu_core)
    add_test(NAME cpu_counter_and_virtual_clock COMMAND cpu_counter_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro")
    add_executable(ipc_domain_tests tests/ipc_domain_tests.cpp)
    target_link_libraries(ipc_domain_tests PRIVATE emu_core)
    add_test(NAME native_applet_domains_and_lifetimes COMMAND ipc_domain_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro")
    add_executable(libnx_app_tests tests/libnx_app_tests.cpp)
    target_link_libraries(libnx_app_tests PRIVATE emu_core)
    add_test(NAME libnx_default_app_and_native_apm COMMAND libnx_app_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_app/program.nro" "${CMAKE_CURRENT_BINARY_DIR}")
    add_executable(libnx_sm_tests tests/libnx_sm_tests.cpp)
    target_link_libraries(libnx_sm_tests PRIVATE emu_core)
    add_test(NAME libnx_original_sm_ipc COMMAND libnx_sm_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_sm/program.nro" "${CMAKE_CURRENT_BINARY_DIR}")
    add_executable(libnx_init_tests tests/libnx_init_tests.cpp)
    target_link_libraries(libnx_init_tests PRIVATE emu_core)
    add_test(NAME libnx_original_init_to_main COMMAND libnx_init_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_init/program.nro" "${CMAKE_CURRENT_BINARY_DIR}")
    add_executable(cpu_movi_tests tests/cpu_movi_tests.cpp)
    target_link_libraries(cpu_movi_tests PRIVATE emu_core)
    add_test(NAME cpu_movi_mask_independent_reference COMMAND cpu_movi_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/movi_reference.bin")
    add_executable(libnx_crt0_tests tests/libnx_crt0_tests.cpp)
    target_link_libraries(libnx_crt0_tests PRIVATE emu_core)
    add_test(NAME libnx_original_crt0_and_relro COMMAND libnx_crt0_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_crt0/program.nro" "${CMAKE_CURRENT_BINARY_DIR}")
    add_executable(nro_bootstrap_tests tests/nro_bootstrap_tests.cpp)
    target_link_libraries(nro_bootstrap_tests PRIVATE emu_core)
    add_test(NAME nro_homebrew_entry_and_validation COMMAND nro_bootstrap_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/nro_bootstrap/program.nro" "${CMAKE_CURRENT_BINARY_DIR}")
    add_executable(libnx_dynamic_tests tests/libnx_dynamic_tests.cpp)
    target_link_libraries(libnx_dynamic_tests PRIVATE emu_core)
    add_test(NAME libnx_upstream_dynamic_runtime COMMAND libnx_dynamic_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/libnx_dynamic/program.elf")
    add_executable(nro_loader_tests tests/nro_loader_tests.cpp)
    target_link_libraries(nro_loader_tests PRIVATE emu_core)
    add_test(NAME nro_loader_and_runtime_boundary COMMAND nro_loader_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/nro_client/program.nro"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/nro_client/program.elf" "${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME nro_compiled_client_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/nro_client/program.nro" 50000 --hle-reference)
    set_tests_properties(nro_compiled_client_cli PROPERTIES PASS_REGULAR_EXPRESSION "Fin normale.*code=0")
    add_executable(hle_input_tests tests/hle_input_tests.cpp)
    target_link_libraries(hle_input_tests PRIVATE emu_core)
    add_test(NAME hle_interactive_input COMMAND hle_input_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_interactive/program.elf"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/root")
    add_executable(hle_animation_tests tests/hle_animation_tests.cpp)
    target_link_libraries(hle_animation_tests PRIVATE emu_core)
    add_test(NAME hle_animation_stream COMMAND hle_animation_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_animation/program.elf"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/root")
    add_test(NAME hle_animation_sequence_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_animation/program.elf" 10000000 --hle-reference
        --read-root "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/root"
        --frames-out "${CMAKE_CURRENT_BINARY_DIR}/animation-cli")
    set_tests_properties(hle_animation_sequence_cli PROPERTIES PASS_REGULAR_EXPRESSION "Images exportees=8 TempsVirtuelNs=700000000")
    add_test(NAME hle_window_requires_hle COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/program.bin" --window)
    set_tests_properties(hle_window_requires_hle PROPERTIES WILL_FAIL TRUE)
    add_executable(hle_graphics_tests tests/hle_graphics_tests.cpp)
    target_link_libraries(hle_graphics_tests PRIVATE emu_core)
    add_test(NAME hle_first_graphics COMMAND hle_graphics_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/program.elf"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/root" "${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME hle_first_graphics_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/program.elf" 1000000 --hle-reference
        --read-root "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/root"
        --frame-out "${CMAKE_CURRENT_BINARY_DIR}/graphics-cli.png")
    set_tests_properties(hle_first_graphics_cli PROPERTIES PASS_REGULAR_EXPRESSION "Image exportee.*160x96")
    add_test(NAME hle_frame_export_requires_frame COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_graphics/program.elf" 1 --hle-reference
        --frame-out "${CMAKE_CURRENT_BINARY_DIR}/no-frame.png")
    set_tests_properties(hle_frame_export_requires_frame PROPERTIES WILL_FAIL TRUE)
    add_executable(hle_file_tests tests/hle_file_tests.cpp)
    target_link_libraries(hle_file_tests PRIVATE emu_core)
    add_test(NAME hle_read_only_files COMMAND hle_file_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_files/program.elf"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_files/root" "${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME hle_read_only_files_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_files/program.elf" 50000 --hle-reference
        --read-root "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_files/root")
    set_tests_properties(hle_read_only_files_cli PROPERTIES PASS_REGULAR_EXPRESSION "Fin normale.*code=0")
    add_test(NAME hle_read_root_requires_hle COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_files/program.elf"
        --read-root "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_files/root")
    set_tests_properties(hle_read_root_requires_hle PROPERTIES WILL_FAIL TRUE)
    add_executable(hle_client_tests tests/hle_client_tests.cpp)
    target_link_libraries(hle_client_tests PRIVATE emu_core)
    add_test(NAME hle_compiled_client_and_controls COMMAND hle_client_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_client/program.elf")
    add_test(NAME hle_compiled_client_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_client/program.elf" 50000 --hle-reference)
    set_tests_properties(hle_compiled_client_cli PROPERTIES PASS_REGULAR_EXPRESSION "Fin normale.*code=0")
    add_executable(hle_sm_tests tests/hle_sm_tests.cpp)
    target_link_libraries(hle_sm_tests PRIVATE emu_core)
    add_test(NAME hle_service_manager COMMAND hle_sm_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_sm/program.elf")
    add_test(NAME hle_service_manager_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_sm/program.elf" 1000 --hle-reference)
    set_tests_properties(hle_service_manager_cli PROPERTIES PASS_REGULAR_EXPRESSION "Fin normale.*code=0")
    add_executable(hle_ipc_buffer_tests tests/hle_ipc_buffer_tests.cpp)
    target_link_libraries(hle_ipc_buffer_tests PRIVATE emu_core)
    add_test(NAME hle_ipc_buffer_transfers COMMAND hle_ipc_buffer_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_ipc_buffers/program.elf")
    add_test(NAME hle_ipc_buffers_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_ipc_buffers/program.elf" 10000 --hle-reference)
    set_tests_properties(hle_ipc_buffers_cli PROPERTIES PASS_REGULAR_EXPRESSION "Fin normale.*code=0")
    add_executable(hle_ipc_tests tests/hle_ipc_tests.cpp)
    target_link_libraries(hle_ipc_tests PRIVATE emu_core)
    add_test(NAME hle_ipc_sessions_and_messages COMMAND hle_ipc_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_ipc/program.elf")
    add_test(NAME hle_ipc_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_ipc/program.elf" 1000 --hle-reference)
    set_tests_properties(hle_ipc_cli PROPERTIES PASS_REGULAR_EXPRESSION "Fin normale.*code=0")
    add_executable(hle_time_tests tests/hle_time_tests.cpp)
    target_link_libraries(hle_time_tests PRIVATE emu_core)
    add_test(NAME hle_virtual_time COMMAND hle_time_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_time/program.elf")
    add_test(NAME hle_virtual_time_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_time/program.elf" 1000 --hle-reference)
    set_tests_properties(hle_virtual_time_cli PROPERTIES PASS_REGULAR_EXPRESSION "Fin normale.*code=0")
    add_executable(hle_thread_tests tests/hle_thread_tests.cpp)
    target_link_libraries(hle_thread_tests PRIVATE emu_core)
    add_test(NAME hle_threads_scheduler COMMAND hle_thread_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_threads/program.elf")
    add_test(NAME hle_threads_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_threads/program.elf" 2000 --hle-reference)
    set_tests_properties(hle_threads_cli PROPERTIES PASS_REGULAR_EXPRESSION "Fin normale.*code=0")
    add_executable(hle_event_tests tests/hle_event_tests.cpp)
    target_link_libraries(hle_event_tests PRIVATE emu_core)
    add_test(NAME hle_events_suspend_resume COMMAND hle_event_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_events/program.elf")
    add_test(NAME hle_events_waiting_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_events/program.elf" 100 --hle-reference)
    set_tests_properties(hle_events_waiting_cli PROPERTIES PASS_REGULAR_EXPRESSION "En attente d'un evenement systeme")
    add_executable(cpu_fp_tests tests/cpu_fp_tests.cpp)
    target_link_libraries(cpu_fp_tests PRIVATE emu_core)
    if(MSVC)
        target_compile_options(cpu_fp_tests PRIVATE /fp:strict)
    else()
        target_compile_options(cpu_fp_tests PRIVATE -frounding-math -ffp-contract=off)
    endif()
    add_test(NAME cpu_fp_simd_independent_reference COMMAND cpu_fp_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_fp/reference.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/program.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_fp/reference-layout.txt")
    add_test(NAME cpu_subnormal_independent_reference COMMAND cpu_fp_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_subnormal/reference.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/program.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_subnormal/reference-layout.txt")
    add_test(NAME cpu_simd_arithmetic_independent_reference COMMAND cpu_fp_tests
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
    add_test(NAME cpu_fp_compiled_native_comparison COMMAND cpu_fp_compiled_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_fp/program.elf")
    add_test(NAME cpu_fp_workload_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_fp/program.elf" 1000 --test-protocol)
    set_tests_properties(cpu_fp_workload_cli PROPERTIES PASS_REGULAR_EXPRESSION "Fin normale.*code=0")
    add_executable(cpu_atomic_tests tests/cpu_atomic_tests.cpp)
    target_link_libraries(cpu_atomic_tests PRIVATE emu_core)
    add_test(NAME cpu_atomic_and_exclusive COMMAND cpu_atomic_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/atomic_vectors.bin")
    add_executable(legacy_tests tests/legacy_tests.cpp)
    target_link_libraries(legacy_tests PRIVATE emu_core)
    add_test(NAME legacy COMMAND legacy_tests)
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE emu_core)
    add_test(NAME core COMMAND core_tests)
    add_test(NAME external_program COMMAND Switch2Emu "${CMAKE_CURRENT_SOURCE_DIR}/program.bin" 4)
    set_tests_properties(external_program PROPERTIES PASS_REGULAR_EXPRESSION "X2=12")
    add_test(NAME missing_program COMMAND Switch2Emu "${CMAKE_CURRENT_BINARY_DIR}/missing-program.bin")
    add_test(NAME invalid_budget COMMAND Switch2Emu "${CMAKE_CURRENT_SOURCE_DIR}/program.bin" -1)
    set_tests_properties(missing_program invalid_budget PROPERTIES WILL_FAIL TRUE)
    add_executable(cpu_pair_tests tests/cpu_pair_tests.cpp)
    target_link_libraries(cpu_pair_tests PRIVATE emu_core)
    add_test(NAME cpu_pairs_and_compiled_program COMMAND cpu_pair_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/pair_vectors.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/stack_sum/program.bin")
    add_test(NAME compiled_program_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/stack_sum/program.bin" 500 --test-protocol)
    set_tests_properties(compiled_program_cli PROPERTIES PASS_REGULAR_EXPRESSION "Fin normale.*code=0")
    add_test(NAME compiled_program_protocol_disabled COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/stack_sum/program.bin" 500)
    set_tests_properties(compiled_program_protocol_disabled PROPERTIES WILL_FAIL TRUE)
    add_executable(elf_loader_tests tests/elf_loader_tests.cpp)
    target_link_libraries(elf_loader_tests PRIVATE emu_core)
    add_test(NAME elf_loader_and_permissions COMMAND elf_loader_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/elf_sum/program.elf"
        "${CMAKE_CURRENT_SOURCE_DIR}/program.bin" "${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME elf_program_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/elf_sum/program.elf" 500 --test-protocol)
    set_tests_properties(elf_program_cli PROPERTIES PASS_REGULAR_EXPRESSION "X2=55")
    add_test(NAME elf_program_protocol_disabled COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/elf_sum/program.elf" 500)
    set_tests_properties(elf_program_protocol_disabled PROPERTIES WILL_FAIL TRUE)
    add_executable(hle_tests tests/hle_tests.cpp)
    target_link_libraries(hle_tests PRIVATE emu_core)
    add_test(NAME hle_process_memory_and_tls COMMAND hle_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_vectors.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_memory/program.elf"
        "${CMAKE_CURRENT_SOURCE_DIR}/program.bin" "${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME hle_program_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_memory/program.elf" 500 --hle-reference)
    set_tests_properties(hle_program_cli PROPERTIES PASS_REGULAR_EXPRESSION "X2=55")
    add_test(NAME hle_program_disabled COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_memory/program.elf" 500)
    set_tests_properties(hle_program_disabled PROPERTIES WILL_FAIL TRUE)
    add_test(NAME conflicting_runtime_options COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/hle_memory/program.elf" --test-protocol --hle-reference)
    set_tests_properties(conflicting_runtime_options PROPERTIES WILL_FAIL TRUE)
    add_executable(cpu_integer_tests tests/cpu_integer_tests.cpp)
    target_link_libraries(cpu_integer_tests PRIVATE emu_core)
    add_test(NAME cpu_integer_independent_reference COMMAND cpu_integer_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_integer/reference.bin")
    add_executable(cpu_compiled_tests tests/cpu_compiled_tests.cpp)
    target_link_libraries(cpu_compiled_tests PRIVATE emu_core)
    add_test(NAME cpu_compiled_native_comparison COMMAND cpu_compiled_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_integer/program.elf")
    add_test(NAME cpu_integer_workload_cli COMMAND Switch2Emu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/cpu_integer/program.elf" 5000 --test-protocol)
    set_tests_properties(cpu_integer_workload_cli PROPERTIES PASS_REGULAR_EXPRESSION "Fin normale.*code=0")
endif()



