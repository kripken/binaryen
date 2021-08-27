(module
  (type $i32-i32 (func (param i32) (result i32)))
  (type $none-f64 (func (result f64)))

  (table $t0 10 10 funcref)
  (table $t1 20 20 (ref null $i32-i32))
  (table $t2 30 30 (ref null $none-f64))

  (elem $e0  (table $t0) (i32.const 0) func $f-i32-i32-1 $f-none-f64-1)
  (elem $e1  (table $t1) (i32.const 0) (ref null $i32-i32) $f-i32-i32-1 $f-i32-i32-2)
  (elem $e2a (table $t2) (i32.const 0) (ref null $none-f64) $f-none-f64-1 $f-none-f64-2)

  ;; Also test an additional segment for one of the tables.
  (elem $e2b (table $t2) (i32.const 0) (ref null $none-f64) $f-none-f64-2 $f-none-f64-1)

  (func $f-i32-i32-1 (param $x i32) (result i32)
    (unreachable)
  )
  (func $f-i32-i32-2 (param $x i32) (result i32)
    (unreachable)
  )

  (func $f-none-f64-1 (result f64)
    (unreachable)
  )
  (func $f-none-f64-2 (result f64)
    (unreachable)
  )
)
