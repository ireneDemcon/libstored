add_executable(components main.cpp)
set_target_properties(components PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
libstored_generate(TARGET components STORES ExampleComponents.st)
install(TARGETS components RUNTIME DESTINATION bin)

