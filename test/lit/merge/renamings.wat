;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.
;; RUN: wasm-merge %s first %s.second.wat second -all -S -o - | filecheck %s

;; Test that we rename items in the second module to avoid name collisions.

(module
  ;; CHECK:      (type $array (array (mut funcref)))
  (type $array (array (mut (ref null func))))

  ;; This tag has a conflict in second.wat, and so second.wat's $foo
  ;; will be renamed.
  ;; CHECK:      (type $none_=>_none (func))

  ;; CHECK:      (type $ref|$array|_=>_none (func (param (ref $array))))

  ;; CHECK:      (type $i32_=>_none (func (param i32)))

  ;; CHECK:      (type $i64_=>_none (func (param i64)))

  ;; CHECK:      (type $f32_=>_none (func (param f32)))

  ;; CHECK:      (type $f64_=>_none (func (param f64)))

  ;; CHECK:      (global $foo i32 (i32.const 1))

  ;; CHECK:      (global $bar i32 (i32.const 2))

  ;; CHECK:      (global $other i32 (i32.const 3))

  ;; CHECK:      (global $bar_2 i32 (i32.const 4))

  ;; CHECK:      (memory $foo 10 20)

  ;; CHECK:      (memory $bar 30 40)

  ;; CHECK:      (memory $foo_2 50 60)

  ;; CHECK:      (memory $other 70 80)

  ;; CHECK:      (data $foo (i32.const 1) "abc")

  ;; CHECK:      (data $bar (i32.const 2) "def")

  ;; CHECK:      (data $other (i32.const 3) "ghi")

  ;; CHECK:      (data $bar_2 (i32.const 4) "jkl")

  ;; CHECK:      (table $foo 10 20 funcref)

  ;; CHECK:      (table $bar 30 40 funcref)

  ;; CHECK:      (table $foo_2 50 60 funcref)

  ;; CHECK:      (table $other 70 80 funcref)

  ;; CHECK:      (elem $foo func $foo $bar)

  ;; CHECK:      (elem $bar func $bar $foo)

  ;; CHECK:      (elem $other func $foo_3 $other)

  ;; CHECK:      (elem $bar_2 func $other $foo_3)

  ;; CHECK:      (tag $foo (param i32))
  (tag $foo (param i32))

  ;; CHECK:      (tag $bar (param i64))
  (tag $bar (param i64))

  ;; This memory has a conflict in second.wat, and so second.wat's $foo
  ;; will be renamed.
  (memory $foo 10 20)

  (memory $bar 30 40)

  (data $foo (i32.const 1) "abc")

  ;; This data segment has a conflict in second.wat, and so second.wat's $bar
  ;; will be renamed.
  (data $bar (i32.const 2) "def")

  ;; This table has a conflict in second.wat, and so second.wat's $foo
  ;; will be renamed.
  (table $foo 10 20 funcref)

  (table $bar 30 40 funcref)

  (elem $foo (ref null func) $foo $bar)

  ;; This global has a conflict in second.wat, and so second.wat's $bar
  ;; will be renamed.
  (elem $bar (ref null func) $bar $foo)

  (global $foo i32 (i32.const 1))

  ;; This global has a conflict in second.wat, and so second.wat's $bar
  ;; will be renamed.
  (global $bar i32 (i32.const 2))

  ;; This export has a conflict in second.wat, and so second.wat's $foo
  ;; will be renamed.
  ;; CHECK:      (tag $foo_2 (param f32))

  ;; CHECK:      (tag $other (param f64))

  ;; CHECK:      (export "foo" (func $foo))
  (export "foo" (func $foo))

  ;; CHECK:      (export "bar" (func $bar))
  (export "bar" (func $bar))

  ;; CHECK:      (export "foo_2" (func $foo_3))

  ;; CHECK:      (export "other" (func $other))

  ;; CHECK:      (export "other-b" (func $other))

  ;; CHECK:      (func $foo (type $none_=>_none)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.const 1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $foo
    ;; This function has a conflict in second.wat, and so second.wat's $foo
    ;; will be renamed.
    (drop
      (i32.const 1)
    )
  )

  ;; CHECK:      (func $bar (type $none_=>_none)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.const 2)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $bar
    (drop
      (i32.const 2)
    )
  )

  ;; CHECK:      (func $uses (type $ref|$array|_=>_none) (param $array (ref $array))
  ;; CHECK-NEXT:  (try $try
  ;; CHECK-NEXT:   (do
  ;; CHECK-NEXT:    (nop)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (catch $foo
  ;; CHECK-NEXT:    (drop
  ;; CHECK-NEXT:     (pop i32)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (try $try0
  ;; CHECK-NEXT:   (do
  ;; CHECK-NEXT:    (nop)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (catch $bar
  ;; CHECK-NEXT:    (drop
  ;; CHECK-NEXT:     (pop i64)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.load $foo
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.load $bar
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (data.drop $foo)
  ;; CHECK-NEXT:  (data.drop $bar)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (table.get $foo
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (table.get $bar
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (array.init_elem $array $foo
  ;; CHECK-NEXT:   (local.get $array)
  ;; CHECK-NEXT:   (i32.const 1)
  ;; CHECK-NEXT:   (i32.const 2)
  ;; CHECK-NEXT:   (i32.const 3)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (array.init_elem $array $bar
  ;; CHECK-NEXT:   (local.get $array)
  ;; CHECK-NEXT:   (i32.const 4)
  ;; CHECK-NEXT:   (i32.const 5)
  ;; CHECK-NEXT:   (i32.const 6)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (global.get $foo)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (global.get $bar)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $foo)
  ;; CHECK-NEXT:  (call $bar)
  ;; CHECK-NEXT: )
  (func $uses (param $array (ref $array))
    ;; Tags.
    (try
      (do)
      (catch $foo
        (drop
          (pop i32)
        )
      )
    )
    (try
      (do)
      (catch $bar
        (drop
          (pop i64)
        )
      )
    )

    ;; Memories
    (drop
      (i32.load $foo
        (i32.const 1)
      )
    )
    (drop
      (i32.load $bar
        (i32.const 2)
      )
    )

    ;; Data segments
    (data.drop $foo)
    (data.drop $bar)

    ;; Tables
    (drop
      (table.get $foo
        (i32.const 1)
      )
    )
    (drop
      (table.get $bar
        (i32.const 2)
      )
    )

    ;; Element segments
    (array.init_elem $array $foo
      (local.get $array)
      (i32.const 1)
      (i32.const 2)
      (i32.const 3)
    )
    (array.init_elem $array $bar
      (local.get $array)
      (i32.const 4)
      (i32.const 5)
      (i32.const 6)
    )

    ;; Globals
    (drop
      (global.get $foo)
    )
    (drop
      (global.get $bar)
    )

    ;; Functions.
    (call $foo)
    (call $bar)
  )
)
;; CHECK:      (func $foo_3 (type $none_=>_none)
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (i32.const 3)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )

;; CHECK:      (func $other (type $none_=>_none)
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (i32.const 4)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )

;; CHECK:      (func $uses.second (type $ref|$array|_=>_none) (param $array (ref $array))
;; CHECK-NEXT:  (try $try
;; CHECK-NEXT:   (do
;; CHECK-NEXT:    (nop)
;; CHECK-NEXT:   )
;; CHECK-NEXT:   (catch $foo_2
;; CHECK-NEXT:    (drop
;; CHECK-NEXT:     (pop f32)
;; CHECK-NEXT:    )
;; CHECK-NEXT:   )
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (try $try0
;; CHECK-NEXT:   (do
;; CHECK-NEXT:    (nop)
;; CHECK-NEXT:   )
;; CHECK-NEXT:   (catch $other
;; CHECK-NEXT:    (drop
;; CHECK-NEXT:     (pop f64)
;; CHECK-NEXT:    )
;; CHECK-NEXT:   )
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (i32.load $foo_2
;; CHECK-NEXT:    (i32.const 3)
;; CHECK-NEXT:   )
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (i32.load $other
;; CHECK-NEXT:    (i32.const 4)
;; CHECK-NEXT:   )
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (data.drop $other)
;; CHECK-NEXT:  (data.drop $bar_2)
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (table.get $foo_2
;; CHECK-NEXT:    (i32.const 3)
;; CHECK-NEXT:   )
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (table.get $other
;; CHECK-NEXT:    (i32.const 4)
;; CHECK-NEXT:   )
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (array.init_elem $array $other
;; CHECK-NEXT:   (local.get $array)
;; CHECK-NEXT:   (i32.const 7)
;; CHECK-NEXT:   (i32.const 8)
;; CHECK-NEXT:   (i32.const 9)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (array.init_elem $array $bar_2
;; CHECK-NEXT:   (local.get $array)
;; CHECK-NEXT:   (i32.const 10)
;; CHECK-NEXT:   (i32.const 11)
;; CHECK-NEXT:   (i32.const 12)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (global.get $other)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (global.get $bar_2)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (call $foo_3)
;; CHECK-NEXT:  (call $other)
;; CHECK-NEXT: )
