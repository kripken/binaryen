(module
  (type $v (func))
  (global $offset i32 (i32.const 1))
  (table 10 10 funcref)
  (elem (global.get $offset) $callee)
  (export "test1" (func $test1))
  (export "test2" (func $test2))
  (func $test1
    (call_indirect (type $v) (i32.const 0)) ;; index 0 is null; should trap
  )
  (func $test2
    (call_indirect (type $v) (i32.const 1)) ;; index 1 has $callee; succeeds
  )
  (func $callee
    (nop)
  )
)
