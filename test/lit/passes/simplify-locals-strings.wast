;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.
;; RUN: wasm-opt %s --simplify-locals -all -S -o - \
;; RUN:   | filecheck %s

(module
  (memory 10 10)

  ;; CHECK:      (type $array (array (mut i8)))
  (type $array (array_subtype (mut i8) data))
  ;; CHECK:      (type $array16 (array (mut i16)))
  (type $array16 (array_subtype (mut i16) data))

  ;; CHECK:      (func $no-new-past-store
  ;; CHECK-NEXT:  (local $temp stringref)
  ;; CHECK-NEXT:  (local.set $temp
  ;; CHECK-NEXT:   (string.new_wtf8 utf8
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (i32.store
  ;; CHECK-NEXT:   (i32.const 3)
  ;; CHECK-NEXT:   (i32.const 4)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $temp)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $temp
  ;; CHECK-NEXT:   (string.new_wtf8 wtf8
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (i32.store
  ;; CHECK-NEXT:   (i32.const 3)
  ;; CHECK-NEXT:   (i32.const 4)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $temp)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $temp
  ;; CHECK-NEXT:   (string.new_wtf8 replace
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (i32.store
  ;; CHECK-NEXT:   (i32.const 3)
  ;; CHECK-NEXT:   (i32.const 4)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $temp)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $temp
  ;; CHECK-NEXT:   (string.new_wtf16
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (i32.store
  ;; CHECK-NEXT:   (i32.const 3)
  ;; CHECK-NEXT:   (i32.const 4)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $temp)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $no-new-past-store
    (local $temp stringref)
    ;; A string.new cannot be moved past a memory store.
    (local.set $temp
      (string.new_wtf8 utf8
        (i32.const 1)
        (i32.const 2)
      )
    )
    (i32.store
      (i32.const 3)
      (i32.const 4)
    )
    (drop
      (local.get $temp)
    )
    (local.set $temp
      (string.new_wtf8 wtf8
        (i32.const 1)
        (i32.const 2)
      )
    )
    (i32.store
      (i32.const 3)
      (i32.const 4)
    )
    (drop
      (local.get $temp)
    )
    (local.set $temp
      (string.new_wtf8 replace
        (i32.const 1)
        (i32.const 2)
      )
    )
    (i32.store
      (i32.const 3)
      (i32.const 4)
    )
    (drop
      (local.get $temp)
    )
    (local.set $temp
      (string.new_wtf16
        (i32.const 1)
        (i32.const 2)
      )
    )
    (i32.store
      (i32.const 3)
      (i32.const 4)
    )
    (drop
      (local.get $temp)
    )
  )

  ;; CHECK:      (func $yes-new-past-store
  ;; CHECK-NEXT:  (local $temp stringref)
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.load
  ;; CHECK-NEXT:    (i32.const 3)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (string.new_wtf8 utf8
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $yes-new-past-store
    (local $temp stringref)
    ;; A string.new can be moved past a memory load.
    (local.set $temp
      (string.new_wtf8 utf8
        (i32.const 1)
        (i32.const 2)
      )
    )
    (drop
      (i32.load
        (i32.const 3)
      )
    )
    (drop
      (local.get $temp)
    )
  )

  ;; CHECK:      (func $no-new-past-store-gc (param $array (ref $array)) (param $array16 (ref $array16))
  ;; CHECK-NEXT:  (local $temp stringref)
  ;; CHECK-NEXT:  (local.set $temp
  ;; CHECK-NEXT:   (string.new_wtf8_array utf8
  ;; CHECK-NEXT:    (local.get $array)
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (i32.store
  ;; CHECK-NEXT:   (i32.const 3)
  ;; CHECK-NEXT:   (i32.const 4)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $temp)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $temp
  ;; CHECK-NEXT:   (string.new_wtf8_array wtf8
  ;; CHECK-NEXT:    (local.get $array)
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (i32.store
  ;; CHECK-NEXT:   (i32.const 3)
  ;; CHECK-NEXT:   (i32.const 4)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $temp)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $temp
  ;; CHECK-NEXT:   (string.new_wtf8_array replace
  ;; CHECK-NEXT:    (local.get $array)
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (i32.store
  ;; CHECK-NEXT:   (i32.const 3)
  ;; CHECK-NEXT:   (i32.const 4)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $temp)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $temp
  ;; CHECK-NEXT:   (string.new_wtf16_array
  ;; CHECK-NEXT:    (local.get $array)
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (i32.store
  ;; CHECK-NEXT:   (i32.const 3)
  ;; CHECK-NEXT:   (i32.const 4)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $temp)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $no-new-past-store-gc (param $array (ref $array)) (param $array16 (ref $array16))
    (local $temp stringref)
    ;; A string.new cannot be moved past a memory store.
    (local.set $temp
      (string.new_wtf8_array utf8
        (local.get $array)
        (i32.const 1)
        (i32.const 2)
      )
    )
    (i32.store
      (i32.const 3)
      (i32.const 4)
    )
    (drop
      (local.get $temp)
    )
    (local.set $temp
      (string.new_wtf8_array wtf8
        (local.get $array)
        (i32.const 1)
        (i32.const 2)
      )
    )
    (i32.store
      (i32.const 3)
      (i32.const 4)
    )
    (drop
      (local.get $temp)
    )
    (local.set $temp
      (string.new_wtf8_array replace
        (local.get $array)
        (i32.const 1)
        (i32.const 2)
      )
    )
    (i32.store
      (i32.const 3)
      (i32.const 4)
    )
    (drop
      (local.get $temp)
    )
    (local.set $temp
      (string.new_wtf16_array
        (local.get $array)
        (i32.const 1)
        (i32.const 2)
      )
    )
    (i32.store
      (i32.const 3)
      (i32.const 4)
    )
    (drop
      (local.get $temp)
    )
  )

  (func $no-load-past-encode (param $ref stringref)
    (local $temp i32)
    (local.set $temp
      (i32.load
        (i32.const 1)
      )
    )
    (drop
      (string.encode_wtf8 wtf8
        (local.get $ref)
        (i32.const 10)
      )
    )
    (drop
      (local.get $temp)
    )
    (local.set $temp
      (i32.load
        (i32.const 1)
      )
    )
    (drop
      (string.encode_wtf8 utf8
        (local.get $ref)
        (i32.const 20)
      )
    )
    (drop
      (local.get $temp)
    )
    (local.set $temp
      (i32.load
        (i32.const 1)
      )
    )
    (drop
      (string.encode_wtf16
        (local.get $ref)
        (i32.const 30)
      )
    )
    (drop
      (local.get $temp)
    )
  )
)
