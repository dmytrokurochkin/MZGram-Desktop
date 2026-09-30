# This file is part of MZGram,
# a fork of Telegram Desktop.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
#
# Headless unit tests for MZGram's own logic that has no session/history/
# network dependency (currently just the Zalgo filter). Deliberately not a
# GUI app like test_text below it: no QApplication, prints PASS/FAIL lines
# and exits non-zero on the first failure, so it can be run in CI with
# nothing more than the built binary.
#
# Built only with -D DESKTOP_APP_TEST_APPS=ON, same as test_text.

add_executable(mzgram_tests)
init_target(mzgram_tests "(tests)")

target_include_directories(mzgram_tests PRIVATE ${src_loc})

nice_target_sources(mzgram_tests ${src_loc}
PRIVATE
    mzgram/mzgram_text_filters.cpp
    mzgram/mzgram_text_filters.h
    mzgram/tests/mzgram_tests_main.cpp
)

target_link_libraries(mzgram_tests
PRIVATE
    desktop-app::lib_base
    desktop-app::external_qt
)

set_target_properties(mzgram_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_dependencies(Telegram mzgram_tests)
