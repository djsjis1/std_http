# 测试可执行文件 (使用内置简易测试框架)
add_executable(http_tests
  test_main.cpp
  test_http_parse.cpp
  test_http_protocol.cpp
)

target_link_libraries(http_tests PRIVATE http::http)

# 注册测试
add_test(NAME http_tests COMMAND http_tests)
