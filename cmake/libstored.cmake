# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

include(CheckIncludeFileCXX)
include(CMakeParseArguments)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(NOT LIBSTORED_SOURCE_DIR)
	get_filename_component(LIBSTORED_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
	set(LIBSTORED_SOURCE_DIR "${LIBSTORED_SOURCE_DIR}" CACHE INTERNAL "")
endif()

if(NOT PYTHON_EXECUTABLE)
	if(CMAKE_HOST_WIN32)
		find_program(PYTHON_EXECUTABLE python REQUIRED)
	else()
		find_program(PYTHON_EXECUTABLE python3 REQUIRED)
	endif()
endif()

# A dummy target that depends on all ...-generated targets.  May be handy in
# case of generating documentation, for example, where all generated header
# files are required.
add_custom_target(all-libstored-generate)

# Create the libstored library based on the generated files.
# Old interface: libstored_lib(libprefix libpath store1 store2 ...)
# New interface: libstored_lib(TARGET lib DESTINATION libpath STORES store1 store1 ... [ZTH] [ZMQ|NO_ZMQ] [QT])
function(libstored_lib libprefix libpath)
	if("${libprefix}" STREQUAL "TARGET")
		cmake_parse_arguments(LIBSTORED_LIB
			"ZTH;ZMQ;NO_ZMQ;QT"
			"TARGET;DESTINATION"
			"STORES"
			${ARGV}
		)
	else()
		message(DEPRECATION "Use keyword-based libstored_lib() instead.")
		set(LIBSTORED_LIB_TARGET ${libprefix}libstored)
		set(LIBSTORED_LIB_DESTINATION ${libpath})
		set(LIBSTORED_LIB_STORES ${ARGN})
	endif()

	# By default use ZMQ.
	set(LIBSTORED_LIB_ZMQ TRUE)

	if(LIBSTORED_LIB_NO_ZMQ)
		set(LIBSTORED_LIB_ZMQ FALSE)
	endif()

	set(LIBSTORED_LIB_TARGET_SRC "")
	foreach(m IN ITEMS ${LIBSTORED_LIB_STORES})
		list(APPEND LIBSTORED_LIB_TARGET_SRC "${LIBSTORED_LIB_DESTINATION}/include/${m}.h")
		list(APPEND LIBSTORED_LIB_TARGET_SRC "${LIBSTORED_LIB_DESTINATION}/src/${m}.cpp")
	endforeach()

	add_library(${LIBSTORED_LIB_TARGET} STATIC
		${LIBSTORED_SOURCE_DIR}/include/stored
		${LIBSTORED_SOURCE_DIR}/include/stored.h
		${LIBSTORED_SOURCE_DIR}/include/stored_config.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/allocator.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/compress.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/config.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/components.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/debugger.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/directory.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/macros.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/poller.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/spm.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/synchronizer.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/types.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/util.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/version.h
		${LIBSTORED_SOURCE_DIR}/include/libstored/zmq.h
		${LIBSTORED_SOURCE_DIR}/src/compress.cpp
		${LIBSTORED_SOURCE_DIR}/src/directory.cpp
		${LIBSTORED_SOURCE_DIR}/src/debugger.cpp
		${LIBSTORED_SOURCE_DIR}/src/poller.cpp
		${LIBSTORED_SOURCE_DIR}/src/protocol.cpp
		${LIBSTORED_SOURCE_DIR}/src/synchronizer.cpp
		${LIBSTORED_SOURCE_DIR}/src/util.cpp
		${LIBSTORED_SOURCE_DIR}/src/zmq.cpp
		${LIBSTORED_LIB_TARGET_SRC}
	)

	target_include_directories(${LIBSTORED_LIB_TARGET} PUBLIC
		$<BUILD_INTERFACE:${LIBSTORED_PREPEND_INCLUDE_DIRECTORIES}>
		$<BUILD_INTERFACE:${LIBSTORED_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${LIBSTORED_LIB_DESTINATION}/include>
		$<INSTALL_INTERFACE:include>
	)

	string(REGEX REPLACE "^(.*)-libstored$" "stored-\\1" libname ${LIBSTORED_LIB_TARGET})
	set_target_properties(${LIBSTORED_LIB_TARGET} PROPERTIES OUTPUT_NAME ${libname})
	target_compile_definitions(${LIBSTORED_LIB_TARGET} PRIVATE -DSTORED_NAME=${libname})

	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -D_DEBUG=1)
	else()
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DNDEBUG=1)
	endif()

	if(MSVC)
		target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE /Wall)
		if(LIBSTORED_DEV)
			target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE /WX)
		endif()
		if(LIBSTORED_DISABLE_RTTI)
			target_compile_options(${LIBSTORED_LIB_TARGET} PUBLIC /GR-)
		endif()
	else()
		target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE -Wall -Wextra -Wdouble-promotion -Wformat=2 -Wundef -Wconversion -ffunction-sections -fdata-sections)
		if(LIBSTORED_DEV)
			target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE -Werror)
		endif()
		if(LIBSTORED_DISABLE_EXCEPTIONS)
			target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE -fno-exceptions)
		endif()
		if(LIBSTORED_DISABLE_RTTI)
			target_compile_options(${LIBSTORED_LIB_TARGET} PUBLIC -fno-rtti)
		endif()
	endif()
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE -Wno-defaulted-function-deleted)
	endif()

	if(LIBSTORED_DRAFT_API)
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_DRAFT_API=1)
	endif()

	CHECK_INCLUDE_FILE_CXX("valgrind/memcheck.h" LIBSTORED_HAVE_VALGRIND)
	if(LIBSTORED_HAVE_VALGRIND)
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_HAVE_VALGRIND=1)
	endif()

	if(LIBSTORED_HAVE_ZTH AND LIBSTORED_LIB_ZTH)
		message(STATUS "Enable Zth integration for ${LIBSTORED_LIB_TARGET}")
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_HAVE_ZTH=1)
		target_link_libraries(${LIBSTORED_LIB_TARGET} PUBLIC libzth)
	endif()

	if(LIBSTORED_HAVE_LIBZMQ AND LIBSTORED_LIB_ZMQ)
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_HAVE_ZMQ=1)
		target_link_libraries(${LIBSTORED_LIB_TARGET} PUBLIC libzmq)
	endif()

	if(LIBSTORED_HAVE_QT AND LIBSTORED_LIB_QT)
		if(TARGET Qt5::Core)
			message(STATUS "Enable Qt5 integration for ${LIBSTORED_LIB_TARGET}")
			target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_HAVE_QT=5)
			target_link_libraries(${LIBSTORED_LIB_TARGET} PUBLIC Qt5::Core)
		else()
			message(STATUS "Enable Qt6 integration for ${LIBSTORED_LIB_TARGET}")
            qt_disable_unicode_defines(${LIBSTORED_LIB_TARGET})
			target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_HAVE_QT=6)
			target_link_libraries(${LIBSTORED_LIB_TARGET} PUBLIC Qt::Core)
		endif()

		set_target_properties(${LIBSTORED_LIB_TARGET} PROPERTIES AUTOMOC ON)
	endif()

	if(WIN32)
		target_link_libraries(${LIBSTORED_LIB_TARGET} INTERFACE ws2_32)
	endif()

	if(LIBSTORED_HAVE_HEATSHRINK)
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PUBLIC -DSTORED_HAVE_HEATSHRINK=1)
		target_link_libraries(${LIBSTORED_LIB_TARGET} PUBLIC heatshrink)
	endif()

	if(${CMAKE_VERSION} VERSION_GREATER "3.6.0")
		find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")
		if(CLANG_TIDY_EXE AND LIBSTORED_CLANG_TIDY)
			message(STATUS "Enabled clang-tidy for ${LIBSTORED_LIB_TARGET}")

			string(CONCAT CLANG_TIDY_CHECKS "-checks="
				"bugprone-*,"
				"-bugprone-easily-swappable-parameters,"
				"-bugprone-macro-parentheses,"
				"-bugprone-reserved-identifier," # Should be fixed.

				"clang-analyzer-*,"

				"-clang-diagnostic-defaulted-function-deleted,"

				"concurrency-*,"

				"cppcoreguidelines-*,"
				"-cppcoreguidelines-avoid-c-arrays,"
				"-cppcoreguidelines-avoid-goto,"
				"-cppcoreguidelines-avoid-magic-numbers,"
				"-cppcoreguidelines-explicit-virtual-functions,"
				"-cppcoreguidelines-macro-usage,"
				"-cppcoreguidelines-pro-bounds-array-to-pointer-decay,"
				"-cppcoreguidelines-pro-bounds-pointer-arithmetic,"
				"-cppcoreguidelines-pro-type-union-access,"
				"-cppcoreguidelines-pro-type-vararg,"

				"hicpp-*,"
				"-hicpp-avoid-c-arrays,"
				"-hicpp-avoid-goto,"
				"-hicpp-braces-around-statements,"
				"-hicpp-member-init,"
				"-hicpp-no-array-decay,"
				"-hicpp-no-malloc,"
				"-hicpp-uppercase-literal-suffix,"
				"-hicpp-use-auto,"
				"-hicpp-use-override,"
				"-hicpp-vararg,"

				"misc-*,"
				"-misc-no-recursion,"
				"-misc-non-private-member-variables-in-classes,"
				"-misc-macro-parentheses,"

				"readability-*,"
				"-readability-braces-around-statements,"
				"-readability-convert-member-functions-to-static,"
				"-readability-else-after-return,"
				"-readability-function-cognitive-complexity,"
				"-readability-identifier-length,"
				"-readability-implicit-bool-conversion,"
				"-readability-magic-numbers,"
				"-readability-make-member-function-const,"
				"-readability-redundant-access-specifiers,"
				"-readability-uppercase-literal-suffix,"

				"performance-*,"
				"-performance-no-int-to-ptr," # Especially on WIN32 HANDLEs.

				"portability-*,"
			)
			set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" "${CLANG_TIDY_CHECKS}"
				"--extra-arg=-I${LIBSTORED_SOURCE_DIR}/include"
				"--extra-arg=-I${CMAKE_BINARY_DIR}/include"
				"--extra-arg=-I${LIBSTORED_LIB_DESTINATION}/include"
				"--header-filter=.*include/libstored.*"
				"--warnings-as-errors=*"
				"--extra-arg=-Wno-unknown-warning-option"
			)

			set_target_properties(${LIBSTORED_LIB_TARGET}
				PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}" )
		else()
			set_target_properties(${LIBSTORED_LIB_TARGET}
				PROPERTIES CXX_CLANG_TIDY "")
		endif()
	endif()

	if(LIBSTORED_ENABLE_ASAN)
		target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PRIVATE -DSTORED_ENABLE_ASAN=1)
		if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
			target_link_options(${LIBSTORED_LIB_TARGET} INTERFACE -fsanitize=address)
		else()
			target_link_libraries(${LIBSTORED_LIB_TARGET} INTERFACE "-fsanitize=address")
		endif()
	endif()

	if(LIBSTORED_ENABLE_LSAN)
		target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE -fsanitize=leak -fno-omit-frame-pointer)
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PRIVATE -DSTORED_ENABLE_LSAN=1)
		if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
			target_link_options(${LIBSTORED_LIB_TARGET} INTERFACE -fsanitize=leak)
		else()
			target_link_libraries(${LIBSTORED_LIB_TARGET} INTERFACE "-fsanitize=leak")
		endif()
	endif()

	if(LIBSTORED_ENABLE_UBSAN)
		target_compile_options(${LIBSTORED_LIB_TARGET} PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
		target_compile_definitions(${LIBSTORED_LIB_TARGET} PRIVATE -DSTORED_ENABLE_UBSAN=1)
		if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
			target_link_options(${LIBSTORED_LIB_TARGET} INTERFACE -fsanitize=undefined)
		else()
			target_link_libraries(${LIBSTORED_LIB_TARGET} INTERFACE "-fsanitize=undefined")
		endif()
	endif()

	if(LIBSTORED_INSTALL_STORE_LIBS)
		install(TARGETS ${LIBSTORED_LIB_TARGET} EXPORT ${LIBSTORED_LIB_TARGET}Store
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
		)

		install(DIRECTORY ${LIBSTORED_LIB_DESTINATION}/doc/ DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/libstored)
		install(EXPORT ${LIBSTORED_LIB_TARGET}Store DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/libstored/cmake)
	endif()
endfunction()

# Not safe against parallel execution if the target directory is used more than once.
function(libstored_copy_dlls target)
	if(WIN32 AND LIBSTORED_HAVE_LIBZMQ)
		get_target_property(target_type ${target} TYPE)
		if(target_type STREQUAL "EXECUTABLE")
			# Missing dll's... Really annoying. Just copy the libzmq.dll to wherever
			# it is possibly needed.
			if(CMAKE_STRIP)
				add_custom_command(TARGET ${target} PRE_LINK
					COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libzmq> $<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:libzmq>
					COMMAND ${CMAKE_STRIP} $<SHELL_PATH:$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:libzmq>>
					VERBATIM
				)
			else()
				add_custom_command(TARGET ${target} PRE_LINK
					COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libzmq> $<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:libzmq>
					VERBATIM
				)
			endif()
		endif()
	endif()
endfunction()

# Generate the store files and invoke libstored_lib to create the library for cmake.
# Old interface: libstored_generate(target store1 store2 ...)
# New interface: libstored_generate(TARGET target [DESTINATION path] STORES store1 store2 [ZTH] [ZMQ|NO_ZMQ])
function(libstored_generate target) # add all other models as varargs
	if("${target}" STREQUAL "TARGET")
		cmake_parse_arguments(LIBSTORED_GENERATE
			"ZTH;ZMQ;NO_ZMQ;QT"
			"TARGET;DESTINATION"
			"STORES"
			${ARGV}
		)
	else()
		message(DEPRECATION "Use keyword-based libstored_generate() instead.")
		set(LIBSTORED_GENERATE_TARGET ${target})
		set(LIBSTORED_GENERATE_STORES ${ARGN})
	endif()

	set(LIBSTORED_GENERATE_FLAGS)
	if(LIBSTORED_GENERATE_ZTH)
		set(LIBSTORED_GENERATE_FLAGS ${LIBSTORED_GENERATE_FLAGS} ZTH)
	endif()
	if(LIBSTORED_GENERATE_ZMQ)
		set(LIBSTORED_GENERATE_FLAGS ${LIBSTORED_GENERATE_FLAGS} ZMQ)
	endif()
	if(LIBSTORED_GENERATE_NO_ZMQ)
		set(LIBSTORED_GENERATE_FLAGS ${LIBSTORED_GENERATE_FLAGS} NO_ZMQ)
	endif()
	if(LIBSTORED_GENERATE_QT)
		set(LIBSTORED_GENERATE_FLAGS ${LIBSTORED_GENERATE_FLAGS} QT)
	endif()

	if("${LIBSTORED_GENERATE_DESTINATION}" STREQUAL "")
		set(LIBSTORED_GENERATE_DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/libstored)
	endif()

	set(model_bases "")
	set(generated_files "")
	foreach(model IN ITEMS ${LIBSTORED_GENERATE_STORES})
		get_filename_component(model_abs "${model}" ABSOLUTE)
		list(APPEND models ${model_abs})
		get_filename_component(model_base "${model}" NAME_WE)
		list(APPEND model_bases ${model_base})
		list(APPEND generated_files ${LIBSTORED_GENERATE_DESTINATION}/include/${model_base}.h)
		list(APPEND generated_files ${LIBSTORED_GENERATE_DESTINATION}/src/${model_base}.cpp)
		list(APPEND generated_files ${LIBSTORED_GENERATE_DESTINATION}/doc/${model_base}.rtf)
		list(APPEND generated_files ${LIBSTORED_GENERATE_DESTINATION}/doc/${model_base}.csv)
		list(APPEND generated_files ${LIBSTORED_GENERATE_DESTINATION}/doc/${model_base}Meta.py)
		list(APPEND generated_files ${LIBSTORED_GENERATE_DESTINATION}/rtl/${model_base}.vhd)
		list(APPEND generated_files ${LIBSTORED_GENERATE_DESTINATION}/rtl/${model_base}_pkg.vhd)
	endforeach()

	add_custom_command(
		OUTPUT ${LIBSTORED_GENERATE_TARGET}-libstored.timestamp ${generated_files}
		DEPENDS ${LIBSTORED_SOURCE_DIR}/include/libstored/store.h.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/src/store.cpp.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/doc/store.rtf.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/doc/store.csv.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/doc/store.py.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/fpga/rtl/store.vhd.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/fpga/rtl/store_pkg.vhd.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/CMakeLists.txt.tmpl
		DEPENDS ${LIBSTORED_SOURCE_DIR}/generator/generate.py
		DEPENDS ${LIBSTORED_SOURCE_DIR}/generator/dsl/grammar.tx
		DEPENDS ${LIBSTORED_SOURCE_DIR}/generator/dsl/types.py
		DEPENDS ${models}
		COMMAND ${PYTHON_EXECUTABLE} ${LIBSTORED_SOURCE_DIR}/generator/generate.py -p ${LIBSTORED_GENERATE_TARGET}- ${models} ${LIBSTORED_GENERATE_DESTINATION}
		COMMAND ${CMAKE_COMMAND} -E touch ${LIBSTORED_GENERATE_TARGET}-libstored.timestamp
		COMMENT "Generate store from ${LIBSTORED_GENERATE_STORES}"
		VERBATIM
	)
	add_custom_target(${LIBSTORED_GENERATE_TARGET}-libstored-generate
		DEPENDS ${LIBSTORED_GENERATE_TARGET}-libstored.timestamp
	)
	add_dependencies(all-libstored-generate ${LIBSTORED_GENERATE_TARGET}-libstored-generate)

	libstored_lib(TARGET ${LIBSTORED_GENERATE_TARGET}-libstored DESTINATION ${LIBSTORED_GENERATE_DESTINATION} STORES ${model_bases} ${LIBSTORED_GENERATE_FLAGS})

	add_dependencies(${LIBSTORED_GENERATE_TARGET}-libstored ${LIBSTORED_GENERATE_TARGET}-libstored-generate)

	get_target_property(target_type ${LIBSTORED_GENERATE_TARGET} TYPE)
	if(target_type MATCHES "^(STATIC_LIBRARY|MODULE_LIBRARY|SHARED_LIBRARY|EXECUTABLE)$")
		target_link_libraries(${LIBSTORED_GENERATE_TARGET} PUBLIC ${LIBSTORED_GENERATE_TARGET}-libstored)
	else()
		add_dependencies(${LIBSTORED_GENERATE_TARGET} ${LIBSTORED_GENERATE_TARGET}-libstored)
	endif()

	get_target_property(target_cxx_standard ${LIBSTORED_GENERATE_TARGET} CXX_STANDARD)
	if(NOT target_cxx_standard STREQUAL "target_cxx_standard-NOTFOUND")
		set_target_properties(${LIBSTORED_GENERATE_TARGET}-libstored PROPERTIES CXX_STANDARD ${target_cxx_standard})
	endif()

	if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
		if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
			if(target_type STREQUAL "EXECUTABLE")
				target_link_options(${LIBSTORED_GENERATE_TARGET} PUBLIC -Wl,--gc-sections)
			endif()
		endif()
	endif()

	if(LIBSTORED_DOCUMENTATION AND TARGET doc)
		add_dependencies(doc ${LIBSTORED_GENERATE_TARGET}-libstored-generate)
	endif()

	libstored_copy_dlls(${LIBSTORED_GENERATE_TARGET})
endfunction()

find_program(RCC_EXE pyside6-rcc PATHS $ENV{HOME}/.local/bin)

cmake_policy(SET CMP0058 NEW)

if(NOT RCC_EXE STREQUAL "RCC_EXE-NOTFOUND")
	function(libstored_visu target rcc)
		foreach(f IN LISTS ARGN)
			get_filename_component(f_abs ${f} ABSOLUTE)

			if(f_abs MATCHES "^(.*/)?main\\.qml$")
				set(qrc_main "${f_abs}")
				string(REGEX REPLACE "^(.*/)?main.qml$" "\\1" qrc_prefix ${f_abs})
			endif()
		endforeach()

		if(NOT qrc_main)
			message(FATAL_ERROR "Missing main.qml input for ${target}")
		endif()

		string(LENGTH "${qrc_prefix}" qrc_prefix_len)

		set(qrc "<!DOCTYPE RCC>\n<RCC version=\"1.0\">\n<qresource>\n")
		foreach(f IN LISTS ARGN)
			get_filename_component(f_abs ${f} ABSOLUTE)
			if(qrc_prefix_len GREATER 0)
				string(SUBSTRING "${f_abs}" 0 ${qrc_prefix_len} f_prefix)
				if(f_prefix STREQUAL qrc_prefix)
					string(SUBSTRING "${f_abs}" ${qrc_prefix_len} -1 f_alias)
					set(qrc "${qrc}<file alias=\"${f_alias}\">${f_abs}</file>\n")
				else()
					set(qrc "${qrc}<file>${f_abs}</file>\n")
				endif()
			else()
				set(qrc "${qrc}<file>${f_abs}</file>\n")
			endif()
		endforeach()
		set(qrc "${qrc}</qresource>\n</RCC>\n")

		get_filename_component(rcc ${rcc} ABSOLUTE)
		file(GENERATE OUTPUT ${rcc}.qrc CONTENT "${qrc}")

		add_custom_command(
			OUTPUT ${rcc}
			DEPENDS
				${LIBSTORED_SOURCE_DIR}/python/libstored/visu/visu.qrc
				${LIBSTORED_SOURCE_DIR}/python/libstored/visu/qml/Libstored/Components/Input.qml
				${LIBSTORED_SOURCE_DIR}/python/libstored/visu/qml/Libstored/Components/Measurement.qml
				${LIBSTORED_SOURCE_DIR}/python/libstored/visu/qml/Libstored/Components/StoreObject.qml
				${LIBSTORED_SOURCE_DIR}/python/libstored/visu/qml/Libstored/Components/qmldir
				${ARGN}
				${rcc}.qrc
			COMMAND ${RCC_EXE} $<SHELL_PATH:${LIBSTORED_SOURCE_DIR}/python/libstored/visu/visu.qrc> $<SHELL_PATH:${rcc}.qrc> -o $<SHELL_PATH:${rcc}>
			COMMENT "Generating ${target} visu"
			VERBATIM
		)

		set_property(SOURCE ${rcc}.qrc ${LIBSTORED_SOURCE_DIR}/python/libstored/visu/visu.qrc
			PROPERTY AUTORCC OFF)
		add_custom_target(${target} DEPENDS ${rcc} SOURCES
			${rcc}.qrc
			${LIBSTORED_SOURCE_DIR}/python/libstored/visu/visu.qrc)
	endfunction()
endif()
