cmake_minimum_required(VERSION 3.16)
project(alarmack_regressions LANGUAGES C CXX)

foreach(name SOURCE PNAL OSAL GTEST GENERATED)
  if(NOT DEFINED ${name})
    message(FATAL_ERROR "Missing -D${name}=absolute-path")
  endif()
endforeach()
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Threads REQUIRED)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/config")
set(config "${CMAKE_BINARY_DIR}/config")

foreach(option FAST_STARTUP PARAMETER_SERVER IR SR REDUNDANCY AR_VENDOR_BLOCKS RS MC_CR)
  set(PNET_OPTION_${option} 1)
endforeach()
foreach(option SRL SNMP DRIVER_ENABLE)
  set(PNET_OPTION_${option} 0)
endforeach()
set(PNET_USE_ATOMICS 0)
set(PNET_MAX_AR 2)
set(PNET_MAX_API 1)
set(PNET_MAX_CR 2)
set(PNET_MAX_SLOTS 5)
set(PNET_MAX_SUBSLOTS 3)
set(PNET_MAX_DFP_IOCR 2)
set(PNET_MAX_PHYSICAL_PORTS 1)
set(PNET_MAX_LOG_BOOK_ENTRIES 16)
set(PNET_MAX_ALARMS 6)
set(PNET_MAX_ALARM_PAYLOAD_DATA_SIZE 28)
set(PNET_MAX_DIAG_ITEMS 200)
set(PNET_MAX_DIAG_MANUF_DATA_SIZE 16)
set(PNET_MAX_MC_CR 1)
set(PNET_MAX_AR_VENDOR_BLOCKS 1)
set(PNET_MAX_AR_VENDOR_BLOCK_DATA_LENGTH 512)
set(PNET_MAX_MAN_SPECIFIC_FAST_STARTUP_DATA_LENGTH 0)
set(PNET_MAX_SESSION_BUFFER_SIZE 4500)
set(PNET_MAX_DIRECTORYPATH_SIZE 240)
set(PNET_MAX_FILENAME_SIZE 30)
set(PNET_MAX_PORT_DESCRIPTION_SIZE 60)
set(LOG_LEVEL FATAL)
foreach(category ETH CPM PPM DCP RPC ALARM AL_BUF LLDP SNMP PNAL)
  set(PF_${category}_LOG OFF)
endforeach()
set(PNET_LOG OFF)
set(PNET_GIT_REVISION "19782d52+local-regression")
set(PNET_VERSION_MAJOR 1)
set(PNET_VERSION_MINOR 0)
set(PNET_VERSION_PATCH 2)
configure_file("${PNAL}/options.h.in" "${config}/options.h")
configure_file("${SOURCE}/pnet_options.h.in" "${config}/pnet_options.h")
configure_file("${SOURCE}/pnet_version.h.in" "${config}/pnet_version.h")

add_library(osal_support STATIC
  "${OSAL}/src/linux/osal.c" "${OSAL}/src/linux/osal_log.c")
target_include_directories(osal_support PUBLIC
  "${OSAL}/include" "${OSAL}/src/linux")
target_link_libraries(osal_support PUBLIC Threads::Threads rt)

# Rebuild only the four old platform units, never older protocol/core code.
add_library(pnal_support STATIC
  "${PNAL}/src/ports/linux/pnal.c"
  "${PNAL}/src/ports/linux/pnal_eth.c"
  "${PNAL}/src/ports/linux/pnal_udp.c"
  "${PNAL}/src/ports/linux/pnal_filetools.c")
target_include_directories(pnal_support PRIVATE
  "${PNAL}/src/ports/linux" "${PNAL}/include" "${PNAL}/src"
  "${PNAL}/src/common" "${PNAL}/src/device" "${config}")
target_link_libraries(pnal_support PUBLIC osal_support)

file(GLOB core CONFIGURE_DEPENDS "${SOURCE}/src/common/*.c" "${SOURCE}/src/device/*.c")
list(FILTER core EXCLUDE REGEX "/pf_snmp\\.c$")
add_library(pnet_core STATIC ${core})
target_include_directories(pnet_core PUBLIC
  "${SOURCE}/include" "${SOURCE}/src" "${SOURCE}/src/common"
  "${SOURCE}/src/device" "${SOURCE}/test" "${config}"
  "${PNAL}/src/ports/linux")
target_compile_definitions(pnet_core PUBLIC UNIT_TEST PNET_STATIC_DEFINE)
target_compile_definitions(pnet_core PRIVATE
  PF_ETH_LOG=LOG_STATE_OFF PF_CPM_LOG=LOG_STATE_OFF PF_PPM_LOG=LOG_STATE_OFF
  PF_DCP_LOG=LOG_STATE_OFF PF_RPC_LOG=LOG_STATE_OFF PF_ALARM_LOG=LOG_STATE_OFF
  PF_AL_BUF_LOG=LOG_STATE_OFF PF_LLDP_LOG=LOG_STATE_OFF PF_SNMP_LOG=LOG_STATE_OFF
  PF_PNAL_LOG=LOG_STATE_OFF PNET_LOG=LOG_STATE_OFF)
target_compile_options(pnet_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(pnet_core PUBLIC osal_support)
include(GenerateExportHeader)
generate_export_header(pnet_core BASE_NAME PNET EXPORT_FILE_NAME "${config}/pnet_export.h")

add_library(gtest_support STATIC
  "${GTEST}/googletest/src/gtest-all.cc" "${GTEST}/googletest/src/gtest_main.cc")
target_include_directories(gtest_support PUBLIC "${GTEST}/googletest/include")
target_include_directories(gtest_support PRIVATE "${GTEST}/googletest")
target_link_libraries(gtest_support PUBLIC Threads::Threads)

add_executable(alarmack_regressions
  "${SOURCE}/test/utils_for_testing.cpp" "${SOURCE}/test/mocks.cpp"
  alarmack_fixture.cpp test_alarmack_parser.cpp test_alarmack_recovery.cpp)
target_include_directories(alarmack_regressions PRIVATE "${GENERATED}")
target_link_libraries(alarmack_regressions PRIVATE pnet_core gtest_support pnal_support)
target_compile_options(alarmack_regressions PRIVATE -Wall -Wextra -Wno-unused-parameter)

enable_testing()
add_test(NAME parser COMMAND alarmack_regressions --gtest_filter=*AlarmAckParser*)
add_test(NAME recovery COMMAND alarmack_regressions --gtest_filter=*AlarmAckRecovery*)
set_tests_properties(parser recovery PROPERTIES TIMEOUT 120)
