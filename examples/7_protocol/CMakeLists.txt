add_executable(7_protocol main.cpp)
libstored_generate(TARGET 7_protocol STORES ExampleProtocol.st)
install(TARGETS 7_protocol RUNTIME DESTINATION bin)

