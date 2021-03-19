(module
 (type $vector (array (mut i32)))
 (type $struct (struct (field (ref null $vector))))
 (import "out" "log" (func $log (param i32)))
 (func $foo (result (ref null $struct))
  (if (result (ref null $struct))
   (i32.const 1)
   (struct.new_with_rtt $struct
    ;; regression test for computing the cost of an array.new_default, which
    ;; lacks the optional field "init"
    (array.new_default_with_rtt $vector
     (i32.const 1)
     (rtt.canon $vector)
    )
    (rtt.canon $struct)
   )
   (ref.null $struct)
  )
 )
 (func $br_on-to-br (param $func (ref func))
  (call $log (i32.const 0))
  (block $null
   ;; a non-null reference is not null, and the br is never taken
   (drop
    (br_on_null $null (ref.func $br_on-to-br))
   )
   (call $log (i32.const 1))
  )
  (call $log (i32.const 2))
  (drop
   (block $func (result funcref)
    ;; a non-null function reference means we always take the br
    (br_on_null $func (ref.func $br_on-to-br))
   )
   (call $log (i32.const 3))
   (ref.func $br_on-to-br)
  )
  (call $log (i32.const 4))
  (drop
   (block $data (result dataref)
    ;; a non-null data reference means we always take the br
    (br_on_null $data
     (array.new_default_with_rtt $vector
      (i32.const 1)
      (rtt.canon $vector)
     )
    )
   )
   (call $log (i32.const 5))
   (array.new_default_with_rtt $vector
    (i32.const 2)
    (rtt.canon $vector)
   )
  )
  (call $log (i32.const 6))
  (drop
   (block $i31 (result i31ref)
    ;; a non-null i31 reference means we always take the br
    (br_on_null $i31
     (i31.new (i32.const 42))
    )
   )
   (call $log (i32.const 7))
   (i31.new (i32.const 1337))
  )
 )
)
