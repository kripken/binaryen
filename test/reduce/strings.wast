(module
  (type $struct (struct (field $string (mut stringref))))

  (type $nnstruct (struct (field $string (mut (ref string)))))

  (global $e stringref (string.const ""))

  (global $x stringref (string.const "x"))

  (global $nx (ref string) (string.const "x"))

  (global $q (ref $struct) (struct.new $struct
    (global.get $x) ;; this should be trampled with an empty string, as it will
                    ;; be written before it is read. XXX or null
  ))

  (global $r (ref $struct) (struct.new $struct
    (global.get $x) ;; this should be trampled with an empty string.
  ))

  (global $t (ref $struct) (struct.new $struct
    (global.get $e) ;; this should be trampled with an empty string.
  ))

  (global $u (ref $struct) (struct.new $struct
    (global.get $x) ;; this should NOT change, as it will be read
  ))

  (global $nn (ref $nnstruct) (struct.new $nnstruct
    (global.get $nx) ;; this should be trampled with an empty string, as it
                     ;; cannot be a null
  ))

  (global $other (ref string) (string.const "other"))

  (global $mut (mut stringref) (ref.null string))

  (func $sets (export "sets")
    (struct.set $struct $string
      (global.get $q)
      (global.get $other)
    )
    (struct.set $struct $string
      (global.get $r)
      (ref.null string)
    )
    (struct.set $struct $string
      (global.get $t)
      (global.get $other)
    )
    (struct.set $nnstruct $string
      (global.get $nn)
      (global.get $other)
    )
    ;; Note, no write to $u.
  )

  (func $q (export "q") (result stringref)
    ;; We wrote to this before, so this prints the global $other.
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

  (func $u (export "u") (result stringref)
    (struct.get $struct $string
      (global.get $u)
    )
  )

  (func $nn (export "nn") (result stringref)
    (struct.get $nnstruct $string
      (global.get $nn)
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

  (func $get-set-mut (export "get-set-mut") (result stringref)
    (local $temp stringref)
    (local.set $temp
      (global.get $mut)
    )
    (global.set $mut
      (string.const "foo")
    )
    (local.get $temp)
  )

  (func $get-mut (export "get-mut") (result stringref)
    (global.get $mut) ;; this stays, it must return "foo".
  )

  (func $get-set-mut-2 (export "get-set-mut-2") (result stringref)
    (local $temp stringref)
    (local.set $temp
      (global.get $mut)
    )
    (global.set $mut
      (string.const "") ;; now we set the empty string
    )
    (local.get $temp)
  )

  (func $get-mut-2 (export "get-mut-2") (result stringref)
    (global.get $mut) ;; this can be optimized to the empty string (and the
                      ;; function then gets merged).
  )
)
