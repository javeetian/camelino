/**
 * @file test_value.c
 * @brief value.h 的单元测试
 * 
 * 编译：gcc -o test_value test_value.c -I../src/core
 * 运行：./test_value
 */

 #include <stdio.h>
 #include <assert.h>
 #include "../src/core/value.h"
 
 // 简单的测试框架
 #define TEST(name) void name(void)
 #define RUN_TEST(name) do { \
     printf("Running %s... ", #name); \
     name(); \
     printf("OK\n"); \
 } while(0)
 #define ASSERT(expr) assert(expr)
 
 // 测试整数编码/解码
 TEST(test_integer_encoding) {
     // 边界值测试
     ASSERT(Caml_val_int(0) == 0);
     ASSERT(Caml_int_val(Caml_val_int(0)) == 0);
     
     ASSERT(Caml_val_int(1) == 2);
     ASSERT(Caml_int_val(Caml_val_int(1)) == 1);
     
     ASSERT(Caml_val_int(-1) == -2);
     ASSERT(Caml_int_val(Caml_val_int(-1)) == -1);
     
     ASSERT(Caml_val_int(12345) == 24690);
     ASSERT(Caml_int_val(Caml_val_int(12345)) == 12345);
     
     // 类型检查
     ASSERT(Caml_is_int(Caml_val_int(42)) == 1);
     ASSERT(Caml_is_ptr(Caml_val_int(42)) == 0);
 }
 
 // 测试指针编码/解码
 TEST(test_pointer_encoding) {
     int x = 42;
     void* p = &x;
     
     caml_value_t v = Caml_val_ptr(p);
     
     ASSERT(Caml_is_ptr(v) == 1);
     ASSERT(Caml_is_int(v) == 0);
     ASSERT(Caml_ptr_val(v) == p);
     ASSERT(Caml_ptr_val(v) == &x);
     
     // 验证地址的最低位确实被用于标记
     // 注意：Caml_ptr_val 已经去掉了标记位，所以 & 1 应该是 0
     // 但 p 本身可能最低位是 0（指针按字对齐），这里只验证去掉标记后是原始指针
     ASSERT(Caml_ptr_val(v) == p);
 }
 
 // 测试头部操作
 TEST(test_header_operations) {
     // 创建头部
     caml_header_t h = Caml_make_header(CAML_TAG_TUPLE, 3);
     ASSERT(Caml_header_tag(h) == CAML_TAG_TUPLE);
     ASSERT(Caml_header_size(h) == 3);
     
     // 测试边界值
     caml_header_t h2 = Caml_make_header(1023, 4194303);
     ASSERT(Caml_header_tag(h2) == 1023);
     ASSERT(Caml_header_size(h2) == 4194303);
 }
 
 // 测试预定义常量
 TEST(test_predefined_constants) {
     ASSERT(CAML_UNIT == 0);
     ASSERT(CAML_TRUE == 2);
     ASSERT(CAML_FALSE == 0);
     ASSERT(CAML_ZERO == 0);
     
     ASSERT(Caml_is_int(CAML_UNIT) == 1);
     ASSERT(Caml_is_int(CAML_TRUE) == 1);
 }
 
 // 测试布尔转换
 TEST(test_bool_conversion) {
     ASSERT(Caml_val_bool(1) == CAML_TRUE);
     ASSERT(Caml_val_bool(0) == CAML_FALSE);
     ASSERT(Caml_bool_val(CAML_TRUE) == 1);
     ASSERT(Caml_bool_val(CAML_FALSE) == 0);
 }
 
 // 测试字符转换
 TEST(test_char_conversion) {
     int c;
     for (c = -128; c < 127; c++) {
         caml_value_t v = Caml_val_char((char)c);
         char c2 = Caml_char_val(v);
         ASSERT(c == c2);
     }
 }
 
 int main(void) {
     printf("=== Camelino Value Module Tests ===\n\n");
     
     RUN_TEST(test_integer_encoding);
     RUN_TEST(test_pointer_encoding);
     RUN_TEST(test_header_operations);
     RUN_TEST(test_predefined_constants);
     RUN_TEST(test_bool_conversion);
     RUN_TEST(test_char_conversion);
     
     printf("\n=== All tests passed! ===\n");
     return 0;
 }