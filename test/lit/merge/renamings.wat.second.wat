(module
  (tag $foo (param i32))

  (tag $other (param i64))

  (global $other i32 (i32.const 3))

  (global $bar i32 (i32.const 4))

  (func $foo
    (drop
      (i32.const 3)
    )
  )

  (func $other
    (drop
      (i32.const 4)
    )
  )
)
