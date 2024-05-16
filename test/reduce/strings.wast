(module
  (global $x stringref (string.const "x"))
  (global $b stringref (string.const "bbb"))
  (global $e stringref (string.const ""))

  (global $n-x (ref string) (string.const "xx"))
  (global $n-b (ref string) (string.const "bbb2"))
  (global $n-e (ref string) (string.const ""))

  (global $g-x stringref (global.get $x))
  (global $g-b stringref (global.get $b))
  (global $g-e stringref (global.get $e))

  (global $n-g-x (ref string) (global.get $n-x))
  (global $n-g-b (ref string) (global.get $n-b))
  (global $n-g-e (ref string) (global.get $n-e))

  (func $x (export "x") (result stringref)
    (string.const "xy")
  )

  (func $b (export "b") (result stringref)
    (string.const "bbb3")
  )

  (func $e (export "e") (result stringref)
    (string.const "")
  )

  (func $gx (export "gx") (result stringref)
    (global.get $x)
  )

  (func $gb (export "gb") (result stringref)
    (global.get $b)
  )

  (func $ge (export "ge") (result stringref)
    (global.get $e)
  )

  (func $ngx (export "ngx") (result stringref)
    (global.get $n-x)
  )

  (func $ngb (export "ngb") (result stringref)
    (global.get $n-b)
  )

  (func $nge (export "nge") (result stringref)
    (global.get $n-e)
  )

  (func $ggx (export "ggx") (result stringref)
    (global.get $g-x)
  )

  (func $ggb (export "ggb") (result stringref)
    (global.get $g-b)
  )

  (func $gge (export "gge") (result stringref)
    (global.get $g-e)
  )
)
