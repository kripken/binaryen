(module
  (type $i32 (struct i32))

  ;; Send values to JS through imports.
  (import "fuzzing-support" "log-i32" (func $log-i32 (param i32)))
  (import "fuzzing-support" "log-f64" (func $log-f64 (param f64)))
  (import "fuzzing-support" "log-anyref" (func $log-anyref (param anyref)))
  (import "fuzzing-support" "log-funcref" (func $log-funcref (param funcref)))
  (import "fuzzing-support" "log-externref" (func $log-externref (param externref)))

  ;; Send values to JS through globals.
  (global $global1 i32 (i32.const 10))
  (global $global2 (mut f64) (f64.const 22.34))

  (export "global-1" (global $global1))
  (export "global-2" (global $global2))

  (func $logging (export "logging")
    (call $log-i32
      (i32.const 42)
    )
    (call $log-f64
      (f64.const 3.14159)
    )
    (call $log-anyref
      (ref.null any)
    )
    (call $log-anyref
      (struct.new $i32
        (i32.const 42)
      )
    )
    (call $log-funcref
      (ref.func $logging)
    )
    (call $log-externref
      (ref.null extern)
    )
  )

  ;; Send values to JS by returning values from exports.
  (func $result-i32 (export "result-i32") (result i32)
    (i32.const 1337)
  )

  (func $result-f64 (export "result-f64") (result f64)
    (f64.const 2.71828)
  )

  (func $result-anyref (export "result-anyref") (result anyref)
    (struct.new $i32
      (i32.const 99)
    )
  )
)

