(module
  (type $struct (struct (field $string (mut stringref))))

  (global $e stringref (string.const ""))

  (global $x stringref (string.const "x"))

  (global $nx (ref string) (string.const "x"))

  (global $q (ref $struct) (struct.new $struct
    (global.get $x) ;; this should be trampled with an empty string.
  ))

  (global $r (ref $struct) (struct.new $struct
    (global.get $x) ;; this should be trampled with an empty string.
  ))

  (global $t (ref $struct) (struct.new $struct
    (global.get $e) ;; this should be trampled with an empty string.
  ))

  (global $other stringref (string.const "other"))

  (func $sets (export "sets")
    (struct.set $struct $string
      (global.get $q)
      (global.get $other)
    )
    (struct.set $struct $string
      (global.get $r)
      (global.get $other)
    )
    (struct.set $struct $string
      (global.get $t)
      (global.get $other)
    )
  )

  (func $q (export "q") (result stringref)
    ;; We wrote to this before, so tihs prints the global $other.
    (struct.get $struct $string
      (global.get $q)
    )
  )

  (func $r (export "r") (result stringref)
    (struct.get $struct $string
      (global.get $r)
    )
  )

  (func $t (export "t") (result stringref)
    (struct.get $struct $string
      (global.get $t)
    )
  )

  (func $get-e (export "get-e") (result stringref)
    (global.get $e)
  )

  (func $get-x (export "get-x") (result stringref)
    (global.get $x)
  )

  (func $get-nx (export "get-nx") (result stringref)
    (global.get $nx)
  )
)
