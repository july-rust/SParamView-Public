cmake_minimum_required(VERSION 3.24)
project(SParamView VERSION 1.4.3 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(SI_BUILD_GUI "Build Qt 6 desktop application" ON)
option(SI_SANITIZE "Enable address/undefined behavior sanitizers" OFF)
option(SI_FUZZ "Build the libFuzzer Touchstone parser harness" OFF)
set(SPARAMVIEW_CORE_PARTITIONS
    src/core_common.cpp src/touchstone.cpp src/cache.cpp src/mapping.cpp
    src/termination.cpp src/trace.cpp src/tdr.cpp src/analysis.cpp
    src/jobs.cpp src/export.cpp)
set_source_files_properties(${SPARAMVIEW_CORE_PARTITIONS} PROPERTIES HEADER_FILE_ONLY TRUE)
add_library(si_core src/core.cpp ${SPARAMVIEW_CORE_PARTITIONS})
target_include_directories(si_core PUBLIC include)
if(MSVC)
  target_compile_options(si_core PUBLIC /utf-8 /W4)
else()
  target_compile_options(si_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(SI_SANITIZE AND NOT MSVC)
  target_compile_options(si_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(si_core PUBLIC -fsanitize=address,undefined)
endif()
find_package(Threads REQUIRED)
target_link_libraries(si_core PUBLIC Threads::Threads)
add_executable(si_cli src/cli.cpp)
target_link_libraries(si_cli PRIVATE si_core)
enable_testing()
# 012_03: independent, Qt-free v1.4.0 shared channel-analysis Core.
add_library(si_channel_analysis src/channel_analysis/common.cpp
                                src/channel_analysis/ild.cpp
                                src/channel_analysis/icn.cpp
                                src/channel_analysis/eye.cpp
                                src/channel_analysis/com.cpp
                                src/channel_analysis/com_120f8.cpp
                                src/channel_analysis/two_port.cpp
                                src/channel_analysis/s_network.cpp
                                src/channel_analysis/package.cpp
                                src/channel_analysis/com_statistics.cpp
                                src/channel_analysis/com_physical_channel.cpp
                                src/channel_analysis/com_causal_package.cpp
                                src/channel_analysis/com_causal_channel.cpp
                                src/channel_analysis/com_interference.cpp
                                src/channel_analysis/com_receiver_noise.cpp
                                src/channel_analysis/com_jitter.cpp
                                src/channel_analysis/com_fixed_noise.cpp
                                src/channel_analysis/com_fixed_fom.cpp
                                src/channel_analysis/com_fixed_tail.cpp
                                src/channel_analysis/com_fixed_ratio.cpp
                                src/channel_analysis/com_eq_candidate_contract.cpp
                                src/channel_analysis/com_eq_search_qualification.cpp
                                src/channel_analysis/com_dfe.cpp
                                src/channel_analysis/com_equalizer_search.cpp
                                src/channel_analysis/com_voltage_ratio.cpp
                                src/channel_analysis/com_joint_tail.cpp
                                src/channel_analysis/com_voltage_scaling.cpp
                                src/channel_analysis/com_victim_isi.cpp
                                src/channel_analysis/com_victim_isi_tail.cpp
                                src/channel_analysis/com_aggressor_voltage.cpp
                                src/channel_analysis/com_combined_tail.cpp
                                src/channel_analysis/com_cursor_tail_ratio.cpp)
target_include_directories(si_channel_analysis PUBLIC include)
target_link_libraries(si_channel_analysis PUBLIC si_core)
if(MSVC)
  target_compile_options(si_channel_analysis PRIVATE /utf-8 /W4)
else()
  target_compile_options(si_channel_analysis PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(si_channel_core_tests tests/channel_analysis/channel_common_tests.cpp)
target_link_libraries(si_channel_core_tests PRIVATE si_channel_analysis)
add_test(NAME channel_common_core COMMAND si_channel_core_tests
         "${CMAKE_BINARY_DIR}/channel-common")
set_tests_properties(channel_common_core PROPERTIES TIMEOUT 90)
install(TARGETS si_channel_core_tests RUNTIME DESTINATION tests)
# 012_05: deterministic ILD oracle/edge qualification independent of Qt.
add_executable(si_ild_tests tests/channel_analysis/ild_tests.cpp)
target_link_libraries(si_ild_tests PRIVATE si_channel_analysis)
add_test(NAME channel_ild_numerics COMMAND si_ild_tests)
set_tests_properties(channel_ild_numerics PROPERTIES TIMEOUT 90)
install(TARGETS si_ild_tests RUNTIME DESTINATION tests)
# 012_06: Qt-free multi-aggressor generic ICN numerical golden qualification.
add_executable(si_icn_tests tests/channel_analysis/icn_tests.cpp)
target_link_libraries(si_icn_tests PRIVATE si_channel_analysis)
add_test(NAME channel_icn_numerics COMMAND si_icn_tests)
set_tests_properties(channel_icn_numerics PROPERTIES TIMEOUT 90)
install(TARGETS si_icn_tests RUNTIME DESTINATION tests)
# 012_07: phase-aware, fail-closed simulated NRZ eye numerical golden tests.
add_executable(si_eye_tests tests/channel_analysis/eye_tests.cpp)
target_link_libraries(si_eye_tests PRIVATE si_channel_analysis)
add_test(NAME channel_eye_numerics COMMAND si_eye_tests)
set_tests_properties(channel_eye_numerics PROPERTIES TIMEOUT 90)
install(TARGETS si_eye_tests RUNTIME DESTINATION tests)
# 012_09: explicit non-normative COM numerical preparation and fail-closed gate.
add_executable(si_com_tests tests/channel_analysis/com_tests.cpp)
target_link_libraries(si_com_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_prequalification COMMAND si_com_tests)
set_tests_properties(channel_com_prequalification PROPERTIES TIMEOUT 90)
install(TARGETS si_com_tests RUNTIME DESTINATION tests)
# 012_09: IEEE 802.3ck-2022 published Table 120F-8 SI parameter
# contract, final-vs-draft FFE/CTLE, physical-package drift and sample-grid tests.
add_executable(si_com_120f8_tests tests/channel_analysis/com_120f8_tests.cpp)
target_link_libraries(si_com_120f8_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_120f8_final_parameters COMMAND si_com_120f8_tests)
set_tests_properties(channel_com_120f8_final_parameters PROPERTIES TIMEOUT 90)
install(TARGETS si_com_120f8_tests RUNTIME DESTINATION tests)
# 012_09: independent analytical reciprocal 2-port network acceptance.
add_executable(si_com_two_port_tests tests/channel_analysis/com_two_port_tests.cpp)
target_link_libraries(si_com_two_port_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_two_port_physics COMMAND si_com_two_port_tests)
set_tests_properties(channel_com_two_port_physics PROPERTIES TIMEOUT 90)
install(TARGETS si_com_two_port_tests RUNTIME DESTINATION tests)
# 012_09: exact 4-port differential-mode extraction and Redheffer network cascade.
add_executable(si_com_s_network_tests tests/channel_analysis/s_network_tests.cpp)
target_link_libraries(si_com_s_network_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_s_network COMMAND si_com_s_network_tests)
set_tests_properties(channel_com_s_network PROPERTIES TIMEOUT 90)
install(TARGETS si_com_s_network_tests RUNTIME DESTINATION tests)
# 012_09: TX/RX balanced package physical circuit analytic regression.
add_executable(si_com_package_tests tests/channel_analysis/com_package_tests.cpp)
target_link_libraries(si_com_package_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_package_physics COMMAND si_com_package_tests)
set_tests_properties(channel_com_package_physics PROPERTIES TIMEOUT 90)
install(TARGETS si_com_package_tests RUNTIME DESTINATION tests)
# 012_09: exact finite-state PAM4/Gaussian tail quantile (not IEEE COM).
add_executable(si_com_statistics_tests tests/channel_analysis/com_statistics_tests.cpp)
target_link_libraries(si_com_statistics_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_statistical_noise COMMAND si_com_statistics_tests)
set_tests_properties(channel_com_statistical_noise PROPERTIES TIMEOUT 90)
install(TARGETS si_com_statistics_tests RUNTIME DESTINATION tests)
# 012_09: two package cases end-to-end S4P->Tx/board/Rx physics oracle.
add_executable(si_com_physical_channel_tests tests/channel_analysis/com_physical_channel_tests.cpp)
target_link_libraries(si_com_physical_channel_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_physical_test_cases COMMAND si_com_physical_channel_tests
         "${CMAKE_BINARY_DIR}/com-physical-source")
set_tests_properties(channel_com_physical_test_cases PROPERTIES TIMEOUT 90)
install(TARGETS si_com_physical_channel_tests RUNTIME DESTINATION tests)
# 012_33: source-backed Test1/Test2 historical causal physical contract,
# independent complex ABCD/FFE/CTLE/RX oracles, negative and stale cases.
add_executable(si_com_physical_contract_012_33_tests
  tests/channel_analysis/com_physical_contract_012_33_tests.cpp)
target_link_libraries(si_com_physical_contract_012_33_tests
  PRIVATE si_channel_analysis)
add_test(NAME channel_com_012_33_physical_contract
  COMMAND si_com_physical_contract_012_33_tests
          "${CMAKE_BINARY_DIR}/com-012_33-source")
set_tests_properties(channel_com_012_33_physical_contract PROPERTIES TIMEOUT 120)
install(TARGETS si_com_physical_contract_012_33_tests RUNTIME DESTINATION tests)
add_executable(si_com_fixed_victim_012_34_tests tests/channel_analysis/com_fixed_victim_012_34_tests.cpp)
target_link_libraries(si_com_fixed_victim_012_34_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_012_34_fixed_victim COMMAND si_com_fixed_victim_012_34_tests "${CMAKE_BINARY_DIR}/com-fixed-victim-012_34")
set_tests_properties(channel_com_012_34_fixed_victim PROPERTIES TIMEOUT 120)
install(TARGETS si_com_fixed_victim_012_34_tests RUNTIME DESTINATION tests)
add_executable(si_com_fixed_crosstalk_012_35_tests tests/channel_analysis/com_fixed_crosstalk_012_35_tests.cpp)
target_link_libraries(si_com_fixed_crosstalk_012_35_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_012_35_fixed_crosstalk COMMAND si_com_fixed_crosstalk_012_35_tests "${CMAKE_BINARY_DIR}/com-fixed-crosstalk-012_35")
set_tests_properties(channel_com_012_35_fixed_crosstalk PROPERTIES TIMEOUT 90)
install(TARGETS si_com_fixed_crosstalk_012_35_tests RUNTIME DESTINATION tests)
add_executable(si_com_fixed_noise_012_36_tests tests/channel_analysis/com_fixed_noise_012_36_tests.cpp)
target_link_libraries(si_com_fixed_noise_012_36_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_fixed_eq_noise_jitter COMMAND si_com_fixed_noise_012_36_tests "${CMAKE_BINARY_DIR}/com-fixed-noise-012_36")
set_tests_properties(channel_com_fixed_eq_noise_jitter PROPERTIES TIMEOUT 120)
install(TARGETS si_com_fixed_noise_012_36_tests RUNTIME DESTINATION tests)
add_executable(si_com_fixed_fom_012_37_tests tests/channel_analysis/com_fixed_fom_012_37_tests.cpp)
target_link_libraries(si_com_fixed_fom_012_37_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_fixed_eq_fom COMMAND si_com_fixed_fom_012_37_tests)
set_tests_properties(channel_com_fixed_eq_fom PROPERTIES TIMEOUT 30)
install(TARGETS si_com_fixed_fom_012_37_tests RUNTIME DESTINATION tests)
add_executable(si_com_fixed_tail_012_38_tests tests/channel_analysis/com_fixed_tail_012_38_tests.cpp)
target_link_libraries(si_com_fixed_tail_012_38_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_fixed_eq_der_tail COMMAND si_com_fixed_tail_012_38_tests)
set_tests_properties(channel_com_fixed_eq_der_tail PROPERTIES TIMEOUT 30)
install(TARGETS si_com_fixed_tail_012_38_tests RUNTIME DESTINATION tests)
add_executable(si_com_fixed_ratio_012_39_tests tests/channel_analysis/com_fixed_ratio_012_39_tests.cpp)
target_link_libraries(si_com_fixed_ratio_012_39_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_fixed_eq_ratio COMMAND si_com_fixed_ratio_012_39_tests)
set_tests_properties(channel_com_fixed_eq_ratio PROPERTIES TIMEOUT 30)
install(TARGETS si_com_fixed_ratio_012_39_tests RUNTIME DESTINATION tests)
add_executable(si_com_eq_candidate_contract_tests tests/channel_analysis/com_eq_candidate_contract_tests.cpp)
target_link_libraries(si_com_eq_candidate_contract_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_eq_candidate_contract COMMAND si_com_eq_candidate_contract_tests "${CMAKE_BINARY_DIR}/com-eq-candidate-012_40")
set_tests_properties(channel_com_eq_candidate_contract PROPERTIES TIMEOUT 30)
install(TARGETS si_com_eq_candidate_contract_tests RUNTIME DESTINATION tests)
add_executable(si_com_eq_search_qualification_tests tests/channel_analysis/com_eq_search_qualification_tests.cpp)
target_link_libraries(si_com_eq_search_qualification_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_eq_search_qualification COMMAND si_com_eq_search_qualification_tests "${CMAKE_BINARY_DIR}/com-eq-search-012_41")
set_tests_properties(channel_com_eq_search_qualification PROPERTIES TIMEOUT 30)
install(TARGETS si_com_eq_search_qualification_tests RUNTIME DESTINATION tests)

# 012_09: independent IEEE 93A causal propagation S/gamma oracles.
add_executable(si_com_causal_package_tests tests/channel_analysis/com_causal_package_tests.cpp)
target_link_libraries(si_com_causal_package_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_causal_package COMMAND si_com_causal_package_tests)
set_tests_properties(channel_com_causal_package PROPERTIES TIMEOUT 90)
install(TARGETS si_com_causal_package_tests RUNTIME DESTINATION tests)
# 012_09: real S4P SourceSnapshot -> historical causal package Test1/2.
add_executable(si_com_causal_channel_tests tests/channel_analysis/com_causal_channel_tests.cpp)
target_link_libraries(si_com_causal_channel_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_causal_s4p_pipeline COMMAND si_com_causal_channel_tests
         "${CMAKE_BINARY_DIR}/com-causal-source")
set_tests_properties(channel_com_causal_s4p_pipeline PROPERTIES TIMEOUT 90)
install(TARGETS si_com_causal_channel_tests RUNTIME DESTINATION tests)
# 012_14: finite-band NEXT/FEXT multi-aggressor, independent analytic oracle.
add_executable(si_com_interference_tests tests/channel_analysis/com_interference_tests.cpp)
target_link_libraries(si_com_interference_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_interference_pulses COMMAND si_com_interference_tests)
set_tests_properties(channel_com_interference_pulses PROPERTIES TIMEOUT 90)
install(TARGETS si_com_interference_tests RUNTIME DESTINATION tests)
# 012_15: bounded Gaussian receiver noise integral and independent PDF/Q oracle.
add_executable(si_com_receiver_noise_tests tests/channel_analysis/com_receiver_noise_tests.cpp)
target_link_libraries(si_com_receiver_noise_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_receiver_gaussian_noise COMMAND si_com_receiver_noise_tests)
set_tests_properties(channel_com_receiver_gaussian_noise PROPERTIES TIMEOUT 90)
install(TARGETS si_com_receiver_noise_tests RUNTIME DESTINATION tests)
# 012_16: bounded local-linear RJ/Dual-Dirac + receiver Gaussian mixture.
add_executable(si_com_jitter_tests tests/channel_analysis/com_jitter_tests.cpp)
target_link_libraries(si_com_jitter_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_jitter COMMAND si_com_jitter_tests)
set_tests_properties(channel_com_jitter PROPERTIES TIMEOUT 90)
install(TARGETS si_com_jitter_tests RUNTIME DESTINATION tests)
# 012_17: fixed DFE on rectangular symbol response, independent numerical oracle.
add_executable(si_com_dfe_tests tests/channel_analysis/com_dfe_tests.cpp)
target_link_libraries(si_com_dfe_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_dfe_fixed_taps COMMAND si_com_dfe_tests)
set_tests_properties(channel_com_dfe_fixed_taps PROPERTIES TIMEOUT 90)
install(TARGETS si_com_dfe_tests RUNTIME DESTINATION tests)
# 012_18: bounded COM FFE/CTLE local search and independent ideal DFE oracle.
add_executable(si_com_equalizer_search_tests
  tests/channel_analysis/com_equalizer_search_tests.cpp)
target_link_libraries(si_com_equalizer_search_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_equalizer_local_search COMMAND si_com_equalizer_search_tests)
set_tests_properties(channel_com_equalizer_local_search PROPERTIES TIMEOUT 90)
install(TARGETS si_com_equalizer_search_tests RUNTIME DESTINATION tests)
# 012_19: isolated non-normative voltage-ratio mathematical golden.
add_executable(si_com_voltage_ratio_tests
  tests/channel_analysis/com_voltage_ratio_tests.cpp)
target_link_libraries(si_com_voltage_ratio_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_voltage_ratio COMMAND si_com_voltage_ratio_tests)
set_tests_properties(channel_com_voltage_ratio PROPERTIES TIMEOUT 30)
install(TARGETS si_com_voltage_ratio_tests RUNTIME DESTINATION tests)
# 012_20: receiver-calibrated non-normative PAM4+Dual-Dirac+Gaussian tails.
add_executable(si_com_joint_tail_tests
  tests/channel_analysis/com_joint_tail_tests.cpp)
target_link_libraries(si_com_joint_tail_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_joint_tail COMMAND si_com_joint_tail_tests)
set_tests_properties(channel_com_joint_tail PROPERTIES TIMEOUT 40)
install(TARGETS si_com_joint_tail_tests RUNTIME DESTINATION tests)
# 012_21: deterministic exploratory pulse -> receiver-equivalent voltage scaling.
add_executable(si_com_voltage_scaling_tests
  tests/channel_analysis/com_voltage_scaling_tests.cpp)
target_link_libraries(si_com_voltage_scaling_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_voltage_scaling COMMAND si_com_voltage_scaling_tests)
set_tests_properties(channel_com_voltage_scaling PROPERTIES TIMEOUT 30)
install(TARGETS si_com_voltage_scaling_tests RUNTIME DESTINATION tests)
# 012_22: bounded exploratory receiver-equivalent victim SELF-ISI statistics.
add_executable(si_com_victim_isi_tests
  tests/channel_analysis/com_victim_isi_tests.cpp)
target_link_libraries(si_com_victim_isi_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_victim_isi COMMAND si_com_victim_isi_tests)
set_tests_properties(channel_com_victim_isi PROPERTIES TIMEOUT 30)
install(TARGETS si_com_victim_isi_tests RUNTIME DESTINATION tests)
# 012_23: bounded non-normative victim self-ISI + Gaussian joint-tail bridge.
add_executable(si_com_victim_isi_tail_tests
  tests/channel_analysis/com_victim_isi_tail_tests.cpp)
target_link_libraries(si_com_victim_isi_tail_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_victim_isi_tail COMMAND si_com_victim_isi_tail_tests)
set_tests_properties(channel_com_victim_isi_tail PROPERTIES TIMEOUT 40)
install(TARGETS si_com_victim_isi_tail_tests RUNTIME DESTINATION tests)
# 012_24: NEXT/FEXT per-aggressor receiver-voltage scaling/qualification.
add_executable(si_com_aggressor_voltage_tests
  tests/channel_analysis/com_aggressor_voltage_tests.cpp)
target_link_libraries(si_com_aggressor_voltage_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_aggressor_voltage COMMAND si_com_aggressor_voltage_tests)
set_tests_properties(channel_com_aggressor_voltage PROPERTIES TIMEOUT 30)
install(TARGETS si_com_aggressor_voltage_tests RUNTIME DESTINATION tests)

# 012_25: victim self-ISI + NEXT/FEXT bounded statistical tail bridge.
add_executable(si_com_combined_tail_tests
  tests/channel_analysis/com_combined_tail_tests.cpp)
target_link_libraries(si_com_combined_tail_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_combined_tail COMMAND si_com_combined_tail_tests)
set_tests_properties(channel_com_combined_tail PROPERTIES TIMEOUT 40)
install(TARGETS si_com_combined_tail_tests RUNTIME DESTINATION tests)
# 012_28: exploratory PAM4 cursor / combined-tail upper-bound dB bridge.
add_executable(si_com_cursor_tail_ratio_tests
  tests/channel_analysis/com_cursor_tail_ratio_tests.cpp)
target_link_libraries(si_com_cursor_tail_ratio_tests PRIVATE si_channel_analysis)
add_test(NAME channel_com_cursor_tail_ratio COMMAND si_com_cursor_tail_ratio_tests)
set_tests_properties(channel_com_cursor_tail_ratio PROPERTIES TIMEOUT 40)
install(TARGETS si_com_cursor_tail_ratio_tests RUNTIME DESTINATION tests)

add_executable(si_hardening_tests tests/hardening_tests.cpp)
target_link_libraries(si_hardening_tests PRIVATE si_core)
install(TARGETS si_hardening_tests RUNTIME DESTINATION tests)
foreach(case snapshot cr_only false_noise valid_noise empty_tdr truncated_live_cache invalid_cached_frequency zero_transfer cancel_reopen parallel_import)
  add_test(NAME hardening_${case} COMMAND si_hardening_tests ${case} "${CMAKE_BINARY_DIR}/hardening/${case}")
  set_tests_properties(hardening_${case} PROPERTIES TIMEOUT 20)
endforeach()
add_executable(si_tests tests/core_tests.cpp)
target_link_libraries(si_tests PRIVATE si_core)
add_test(NAME core COMMAND si_tests)
add_executable(si_v116_regression_tests tests/v116_regression_tests.cpp)
target_link_libraries(si_v116_regression_tests PRIVATE si_core)
add_test(NAME v116_core_hardening COMMAND si_v116_regression_tests)
install(TARGETS si_v116_regression_tests RUNTIME DESTINATION tests)
add_executable(si_v121_regression_tests tests/v121_regression_tests.cpp)
target_link_libraries(si_v121_regression_tests PRIVATE si_core)
add_test(NAME v121_ranking_tdr_range COMMAND si_v121_regression_tests)
install(TARGETS si_v121_regression_tests RUNTIME DESTINATION tests)
add_executable(si_algorithm_regression_tests tests/algorithm_regression_tests.cpp)
target_link_libraries(si_algorithm_regression_tests PRIVATE si_core)
add_test(NAME algorithm_boundary_regressions COMMAND si_algorithm_regression_tests)
install(TARGETS si_algorithm_regression_tests RUNTIME DESTINATION tests)
add_executable(si_direction_tests tests/direction_tests.cpp)
target_link_libraries(si_direction_tests PRIVATE si_core)
add_test(NAME bidirectional_direction_delta COMMAND si_direction_tests "${CMAKE_BINARY_DIR}/direction-test")
install(TARGETS si_direction_tests RUNTIME DESTINATION tests)
add_executable(si_golden_tests tests/golden_tests.cpp)
target_link_libraries(si_golden_tests PRIVATE si_core)
add_test(NAME golden_touchstone_numerics COMMAND si_golden_tests "${CMAKE_BINARY_DIR}/golden-test")
install(TARGETS si_golden_tests RUNTIME DESTINATION tests)
if(SI_FUZZ)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "SI_FUZZ requires Clang/libFuzzer")
  endif()
  add_executable(si_touchstone_fuzz tests/touchstone_fuzz.cpp)
  target_link_libraries(si_touchstone_fuzz PRIVATE si_core)
  target_compile_options(si_touchstone_fuzz PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
  target_link_options(si_touchstone_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
add_executable(si_mapping_tests tests/mapping_tests.cpp)
target_link_libraries(si_mapping_tests PRIVATE si_core)
add_test(NAME mapping_and_sign COMMAND si_mapping_tests)
add_executable(si_polarity_tests tests/polarity_tests.cpp)
target_link_libraries(si_polarity_tests PRIVATE si_core)
add_test(NAME polarity_token_mapping COMMAND si_polarity_tests)
install(TARGETS si_polarity_tests RUNTIME DESTINATION tests)
if(SI_BUILD_GUI)
  set(CMAKE_AUTORCC ON)
  find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Concurrent)
  include(cmake/JapaneseFont.cmake)
  add_executable(si_session_cache_tests tests/session_cache_tests.cpp)
  target_include_directories(si_session_cache_tests PRIVATE src)
  target_compile_definitions(si_session_cache_tests PRIVATE SPARAMVIEW_CACHE_TEST_FAULTS=1)
  target_link_libraries(si_session_cache_tests PRIVATE si_core Qt6::Widgets)
  install(TARGETS si_session_cache_tests RUNTIME DESTINATION tests)
  add_test(NAME session_cache_lifecycle COMMAND si_session_cache_tests)
  set_tests_properties(session_cache_lifecycle PROPERTIES TIMEOUT 45)
  # Headless-safe error reporting is tested separately from GUI construction.
  add_executable(si_v133_startup_tests tests/v133_startup_tests.cpp)
  target_include_directories(si_v133_startup_tests PRIVATE src)
  target_link_libraries(si_v133_startup_tests PRIVATE Qt6::Widgets)
  add_test(NAME v133_startup_failure COMMAND si_v133_startup_tests)
  set_tests_properties(v133_startup_failure PROPERTIES
                       ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)
  install(TARGETS si_v133_startup_tests RUNTIME DESTINATION tests)
  add_executable(si_multi_process_cache_tests tests/multi_process_cache_tests.cpp)
  target_include_directories(si_multi_process_cache_tests PRIVATE src)
  target_link_libraries(si_multi_process_cache_tests PRIVATE si_core Qt6::Core)
  install(TARGETS si_multi_process_cache_tests RUNTIME DESTINATION tests)
  add_test(NAME multi_process_cache_isolation COMMAND si_multi_process_cache_tests)
  set_tests_properties(multi_process_cache_isolation PROPERTIES TIMEOUT 90)
  add_library(si_qt_workers src/workers.cpp)
  target_include_directories(si_qt_workers PUBLIC src)
  target_link_libraries(si_qt_workers PUBLIC Qt6::Concurrent)
  # 012_04: non-blocking, feature-isolated Analysis windows and Qt worker scheduler.
  add_library(si_analysis_gui src/gui/analysis/analysis_windows.cpp)
  target_include_directories(si_analysis_gui PUBLIC src)
  target_link_libraries(si_analysis_gui PUBLIC si_channel_analysis Qt6::Widgets Qt6::Concurrent)
  add_executable(si_channel_gui_tests tests/channel_analysis/channel_gui_tests.cpp)
  target_link_libraries(si_channel_gui_tests PRIVATE si_analysis_gui Qt6::Widgets)
  add_test(NAME channel_analysis_window_lifecycle COMMAND si_channel_gui_tests)
  set_tests_properties(channel_analysis_window_lifecycle PROPERTIES
                       ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
  install(TARGETS si_channel_gui_tests RUNTIME DESTINATION tests)
  add_executable(si_qt_tests tests/qt_tests.cpp src/plot.cpp src/theme.cpp src/navigation.cpp)
  target_link_libraries(si_qt_tests PRIVATE si_core si_qt_workers Qt6::Widgets)
  add_test(NAME qt_workers_and_labels COMMAND si_qt_tests)
  set_tests_properties(qt_workers_and_labels PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
  set(SPARAMVIEW_WINDOW_PARTITIONS
      src/window_ui.cpp src/window_workspace.cpp src/window_mapping.cpp
      src/window_analysis.cpp src/window_results.cpp src/window_plot_controller.cpp
      src/window_project.cpp src/window_export.cpp src/window_validation.cpp)
  set_source_files_properties(${SPARAMVIEW_WINDOW_PARTITIONS} PROPERTIES HEADER_FILE_ONLY TRUE)
  set(SPARAMVIEW_SOURCES
      src/main.cpp src/window.cpp ${SPARAMVIEW_WINDOW_PARTITIONS}
      src/plot.cpp src/report.cpp src/theme.cpp src/navigation.cpp assets/SParamView.qrc)
  if(WIN32)
    list(APPEND SPARAMVIEW_SOURCES assets/SParamView.rc)
  endif()
  qt_add_executable(SParamView ${SPARAMVIEW_SOURCES})
  target_link_libraries(SParamView PRIVATE si_core si_qt_workers si_analysis_gui Qt6::Widgets)
  add_custom_command(TARGET SParamView POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SParamView>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SPARAMVIEW_JP_FONT}" "$<TARGET_FILE_DIR:SParamView>/fonts/")
  # Fault hooks cannot be enabled by environment or arguments in SParamView.
  qt_add_executable(si_failure_probe ${SPARAMVIEW_SOURCES})
  set_target_properties(si_failure_probe PROPERTIES WIN32_EXECUTABLE FALSE)
  target_compile_definitions(si_failure_probe PRIVATE SPARAMVIEW_QUALIFICATION_BUILD=1)
  target_link_libraries(si_failure_probe PRIVATE si_core si_qt_workers si_analysis_gui Qt6::Widgets)
  install(TARGETS si_failure_probe RUNTIME DESTINATION tests)

  if(MINGW)
    target_compile_options(SParamView PRIVATE -fstack-protector-strong)
    target_link_options(SParamView PRIVATE -fstack-protector-strong)
  endif()
  set_target_properties(SParamView PROPERTIES WIN32_EXECUTABLE TRUE OUTPUT_NAME "SParamView")
  install(TARGETS SParamView RUNTIME DESTINATION .)
  install(TARGETS si_qt_tests RUNTIME DESTINATION tests)
endif()
install(TARGETS si_cli RUNTIME DESTINATION tools)
install(TARGETS si_tests si_mapping_tests RUNTIME DESTINATION tests)
install(DIRECTORY examples docs DESTINATION .)

install(DIRECTORY assets/fonts DESTINATION .)
if(WIN32)
  install(DIRECTORY assets/fonts DESTINATION tests)
endif()

add_executable(si_real_probe tests/real_file_probe.cpp)
target_link_libraries(si_real_probe PRIVATE si_core)
add_executable(si_termination_tests tests/termination_tests.cpp)
target_link_libraries(si_termination_tests PRIVATE si_core)
add_test(NAME termination_circuits COMMAND si_termination_tests)
add_executable(si_termination_probe tests/termination_probe.cpp)
target_link_libraries(si_termination_probe PRIVATE si_core)
install(TARGETS si_termination_tests RUNTIME DESTINATION tests)

add_executable(si_performance_tests tests/performance_tests.cpp)
target_link_libraries(si_performance_tests PRIVATE si_core)
add_test(NAME tdr_cache_invariants COMMAND si_performance_tests)
install(TARGETS si_performance_tests RUNTIME DESTINATION tests)

add_executable(si_mapping_policy_tests tests/mapping_policy_tests.cpp)
target_link_libraries(si_mapping_policy_tests PRIVATE si_core)
add_test(NAME mapping_confirmation_policy COMMAND si_mapping_policy_tests)
install(TARGETS si_mapping_policy_tests RUNTIME DESTINATION tests)
if(SI_BUILD_GUI)
  add_executable(si_header_ui_tests tests/header_ui_tests.cpp src/window.cpp src/report.cpp src/plot.cpp src/navigation.cpp src/theme.cpp)
  target_link_libraries(si_header_ui_tests PRIVATE si_core si_qt_workers si_analysis_gui Qt6::Widgets)
  add_test(NAME header_project_buttons COMMAND si_header_ui_tests)
  set_tests_properties(header_project_buttons PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
  install(TARGETS si_header_ui_tests RUNTIME DESTINATION tests)
  add_executable(si_workspace_tests tests/workspace_tests.cpp src/window.cpp src/report.cpp src/plot.cpp src/navigation.cpp src/theme.cpp)
  target_link_libraries(si_workspace_tests PRIVATE si_core si_qt_workers si_analysis_gui Qt6::Widgets)
  add_custom_command(TARGET si_workspace_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/fonts"
            "$<TARGET_FILE_DIR:si_workspace_tests>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SPARAMVIEW_JP_FONT}" "$<TARGET_FILE_DIR:si_workspace_tests>/fonts/"
    COMMENT "Mirror deployed fonts for deterministic workspace layout testing")
  add_test(NAME workspace_layout COMMAND si_workspace_tests "${CMAKE_SOURCE_DIR}/examples/demo.siproject" "${CMAKE_BINARY_DIR}/workspace-test")
  set_tests_properties(workspace_layout PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
  install(TARGETS si_workspace_tests RUNTIME DESTINATION tests)
  add_executable(si_navigation_tests tests/navigation_tests.cpp src/window.cpp src/report.cpp src/plot.cpp src/navigation.cpp src/theme.cpp)
  target_link_libraries(si_navigation_tests PRIVATE si_core si_qt_workers si_analysis_gui Qt6::Widgets)
  target_compile_definitions(si_navigation_tests PRIVATE SI_NEW_NAVIGATION)
  add_test(NAME navigation_responsiveness COMMAND si_navigation_tests "${CMAKE_BINARY_DIR}/navigation-test")
  set_tests_properties(navigation_responsiveness PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
  install(TARGETS si_navigation_tests RUNTIME DESTINATION tests)
  add_executable(si_open_tests tests/open_tests.cpp src/window.cpp src/report.cpp src/plot.cpp src/navigation.cpp src/theme.cpp)
  target_link_libraries(si_open_tests PRIVATE si_core si_qt_workers si_analysis_gui Qt6::Widgets)
  add_test(NAME mapping_fallback_gui COMMAND si_open_tests "${CMAKE_BINARY_DIR}/open-test" fallback)
  set_tests_properties(mapping_fallback_gui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
  add_test(NAME signal_type_detection_gui COMMAND si_open_tests "${CMAKE_BINARY_DIR}/signal-type-test" signaltype)
  set_tests_properties(signal_type_detection_gui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
  install(TARGETS si_open_tests RUNTIME DESTINATION tests)
endif()

add_executable(si_v124_regression_tests tests/v124_regression_tests.cpp)
target_link_libraries(si_v124_regression_tests PRIVATE si_core)
foreach(case named options triangle hash numbers)
  add_test(NAME v124_${case} COMMAND si_v124_regression_tests ${case})
endforeach()
install(TARGETS si_v124_regression_tests RUNTIME DESTINATION tests)

if(SI_BUILD_GUI)
  add_executable(si_v124_gui_tests tests/v124_gui_tests.cpp src/window.cpp src/report.cpp src/plot.cpp src/navigation.cpp src/theme.cpp)
  target_link_libraries(si_v124_gui_tests PRIVATE si_core si_qt_workers si_analysis_gui Qt6::Widgets)
  foreach(case reuse dirty project_changed single_review mixed transition roundtrip workspace_switch cleanup)
    add_test(NAME v124_gui_${case} COMMAND si_v124_gui_tests ${case} "${CMAKE_BINARY_DIR}/v124-${case}")
    set_tests_properties(v124_gui_${case} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
  endforeach()
  install(TARGETS si_v124_gui_tests RUNTIME DESTINATION tests)
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME localization_catalog_source_guard
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/check_localization_catalog.py")
  set_tests_properties(localization_catalog_source_guard PROPERTIES TIMEOUT 30)
  # 012_09: signed-profile evidence is not satisfied by synthetic 012_02 oracles.
  add_test(NAME channel_com_normative_gate
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/tests/channel_analysis"
            -p "test_com_reference_preflight.py" -v)
  set_tests_properties(channel_com_normative_gate PROPERTIES TIMEOUT 45)
  # 012_11: verify code constants against two published-edition text crosschecks.
  # Numeric-profile regression is NOT IEEE COM numerical conformance.
  add_test(NAME com_120f8_published_parameter_contract
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/tests/channel_analysis"
            -p "test_com_120f8_published_profile.py" -v)
  set_tests_properties(com_120f8_published_parameter_contract PROPERTIES TIMEOUT 30)
  # 012_12: final source-verified 120F-8 -> reference-config QA contract.
  # Static acceptance does not certify IEEE COM or external runtime execution.
  add_test(NAME com_120f8_reference_config_contract
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/tests/channel_analysis"
            -p "test_com_120f8_config_contract.py" -v)
  set_tests_properties(com_120f8_reference_config_contract PROPERTIES TIMEOUT 30)
  add_test(NAME cli_session_cache_cleanup
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/check_cli_temp_cleanup.py" "$<TARGET_FILE:si_cli>")
  set_tests_properties(cli_session_cache_cleanup PROPERTIES TIMEOUT 45)
  add_test(NAME release_publication_safety COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/publish_release_tests.py")
endif()

# v1.3.0 specification and vendor-port regression coverage
add_executable(si_v130_touchstone_tests tests/v130_touchstone_tests.cpp)
target_link_libraries(si_v130_touchstone_tests PRIVATE si_core)
add_test(NAME v130_touchstone_compatibility COMMAND si_v130_touchstone_tests)
install(TARGETS si_v130_touchstone_tests RUNTIME DESTINATION tests)

# v1.3.3 deterministic parser boundary and compatibility regressions
add_executable(si_v133_parser_tests tests/v133_parser_tests.cpp)
target_link_libraries(si_v133_parser_tests PRIVATE si_core)
add_test(NAME v133_parser_hardening COMMAND si_v133_parser_tests)
set_tests_properties(v133_parser_hardening PROPERTIES TIMEOUT 30)
install(TARGETS si_v133_parser_tests RUNTIME DESTINATION tests)

# v1.3.3 Stage 2 CSV formula-safety and lossless Mapping CSV regressions
add_executable(si_v133_csv_tests tests/v133_csv_tests.cpp)
target_link_libraries(si_v133_csv_tests PRIVATE si_core)
add_test(NAME v133_csv_roundtrip_security COMMAND si_v133_csv_tests "${CMAKE_BINARY_DIR}/v133-csv")
set_tests_properties(v133_csv_roundtrip_security PROPERTIES TIMEOUT 30)
install(TARGETS si_v133_csv_tests RUNTIME DESTINATION tests)

add_executable(si_spreadsheet_probe tests/spreadsheet_probe.cpp)
target_link_libraries(si_spreadsheet_probe PRIVATE si_core)
install(TARGETS si_spreadsheet_probe RUNTIME DESTINATION tests)
