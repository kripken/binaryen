(module
 (func $yes (param $x i64) (result f32)
  (f32.convert_i64_u (local.get $x))
 )
 (func $no (param $x i32) (result f32)
  (f32.convert_i32_u (local.get $x))
 )
 (func $yes-unreach (result f32)
  (f32.convert_i64_u (unreachable))
 )
 (func $no-unreach (result f32)
  (f32.convert_i32_u (unreachable))
 )
)
