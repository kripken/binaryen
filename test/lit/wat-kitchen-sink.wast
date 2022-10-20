;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.

;; RUN: wasm-opt --new-wat-parser --hybrid -all %s -S -o - | filecheck %s

(module $parse
 ;; types

 ;; CHECK:      (type $none_=>_i32 (func_subtype (result i32) func))

 ;; CHECK:      (type $ret2 (func_subtype (result i32 i32) func))
 (type $ret2 (func (result i32 i32)))

 (rec
  ;; CHECK:      (type $void (func_subtype func))

  ;; CHECK:      (type $i32_=>_none (func_subtype (param i32) func))

  ;; CHECK:      (type $v128_i32_=>_v128 (func_subtype (param v128 i32) (result v128) func))

  ;; CHECK:      (type $many (func_subtype (param i32 i64 f32 f64) (result anyref (ref func)) func))

  ;; CHECK:      (type $i32_i32_=>_none (func_subtype (param i32 i32) func))

  ;; CHECK:      (type $i32_i32_f64_f64_=>_none (func_subtype (param i32 i32 f64 f64) func))

  ;; CHECK:      (type $i64_=>_none (func_subtype (param i64) func))

  ;; CHECK:      (type $i32_i32_i32_=>_none (func_subtype (param i32 i32 i32) func))

  ;; CHECK:      (type $i32_i64_=>_none (func_subtype (param i32 i64) func))

  ;; CHECK:      (type $v128_=>_i32 (func_subtype (param v128) (result i32) func))

  ;; CHECK:      (type $v128_v128_=>_v128 (func_subtype (param v128 v128) (result v128) func))

  ;; CHECK:      (type $v128_v128_v128_=>_v128 (func_subtype (param v128 v128 v128) (result v128) func))

  ;; CHECK:      (rec
  ;; CHECK-NEXT:  (type $s0 (struct_subtype  data))
  (type $s0 (sub (struct)))
  ;; CHECK:       (type $s1 (struct_subtype  data))
  (type $s1 (struct (field)))
 )

 (rec)

 ;; CHECK:      (type $s2 (struct_subtype (field i32) data))
 (type $s2 (struct i32))
 ;; CHECK:      (type $s3 (struct_subtype (field i64) data))
 (type $s3 (struct (field i64)))
 ;; CHECK:      (type $s4 (struct_subtype (field $x f32) data))
 (type $s4 (struct (field $x f32)))
 ;; CHECK:      (type $s5 (struct_subtype (field i32) (field i64) data))
 (type $s5 (struct i32 i64))
 ;; CHECK:      (type $s6 (struct_subtype (field i64) (field f32) data))
 (type $s6 (struct (field i64 f32)))
 ;; CHECK:      (type $s7 (struct_subtype (field $x f32) (field $y f64) data))
 (type $s7 (struct (field $x f32) (field $y f64)))
 ;; CHECK:      (type $s8 (struct_subtype (field i32) (field i64) (field $z f32) (field f64) (field (mut i32)) data))
 (type $s8 (struct i32 (field) i64 (field $z f32) (field f64 (mut i32))))

 ;; CHECK:      (type $a0 (array_subtype i32 data))
 (type $a0 (array i32))
 ;; CHECK:      (type $a1 (array_subtype i64 data))
 (type $a1 (array (field i64)))
 ;; CHECK:      (type $a2 (array_subtype (mut f32) data))
 (type $a2 (array (mut f32)))
 ;; CHECK:      (type $a3 (array_subtype (mut f64) data))
 (type $a3 (array (field $x (mut f64))))

 (rec
   (type $void (func))
 )

 ;; CHECK:      (type $subvoid (func_subtype $void))
 (type $subvoid (sub $void (func)))

 (type $many (func (param $x i32) (param i64 f32) (param) (param $y f64)
                   (result anyref (ref func))))

 ;; CHECK:      (type $submany (func_subtype (param i32 i64 f32 f64) (result anyref (ref func)) $many))
 (type $submany (sub $many (func (param i32 i64 f32 f64) (result anyref (ref func)))))

 ;; globals
 (global $g1 (export "g1") (export "g1.1") (import "mod" "g1") i32)
 (global $g2 (import "mod" "g2") (mut i64))
 (global (import "" "g3") (ref 0))
 (global (import "mod" "") (ref null $many))

 (global (mut i32) i32.const 0)
 ;; CHECK:      (type $ref|$s0|_ref|$s1|_ref|$s2|_ref|$s3|_ref|$s4|_ref|$s5|_ref|$s6|_ref|$s7|_ref|$s8|_ref|$a0|_ref|$a1|_ref|$a2|_ref|$a3|_ref|$subvoid|_ref|$submany|_=>_none (func_subtype (param (ref $s0) (ref $s1) (ref $s2) (ref $s3) (ref $s4) (ref $s5) (ref $s6) (ref $s7) (ref $s8) (ref $a0) (ref $a1) (ref $a2) (ref $a3) (ref $subvoid) (ref $submany)) func))

 ;; CHECK:      (import "" "mem" (memory $mimport$1 0))

 ;; CHECK:      (import "mod" "g1" (global $g1 i32))

 ;; CHECK:      (import "mod" "g2" (global $g2 (mut i64)))

 ;; CHECK:      (import "" "g3" (global $gimport$0 (ref $ret2)))

 ;; CHECK:      (import "mod" "" (global $gimport$1 (ref null $many)))

 ;; CHECK:      (import "mod" "f5" (func $fimport$1))

 ;; CHECK:      (global $2 (mut i32) (i32.const 0))

 ;; CHECK:      (global $i32 i32 (i32.const 42))
 (global $i32 i32 i32.const 42)

 ;; memories
 ;; CHECK:      (memory $mem 1 1)
 (memory $mem 1 1)
 (memory 0)
 ;; CHECK:      (memory $0 0)

 ;; CHECK:      (memory $mem-i32 0 1)
 (memory $mem-i32 i32 0 1)
 ;; CHECK:      (memory $mem-i64 i64 2)
 (memory $mem-i64 i64 2)
 (memory (export "mem") (export "mem2") (import "" "mem") 0)
 ;; CHECK:      (memory $mem-init 1 1)
 (memory $mem-init (data "hello, world!"))

 ;; functions
 (func)

 ;; CHECK:      (export "g1" (global $g1))

 ;; CHECK:      (export "g1.1" (global $g1))

 ;; CHECK:      (export "mem" (memory $mimport$1))

 ;; CHECK:      (export "mem2" (memory $mimport$1))

 ;; CHECK:      (export "f5.0" (func $fimport$1))

 ;; CHECK:      (export "f5.1" (func $fimport$1))

 ;; CHECK:      (func $0 (type $void)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT: )

 ;; CHECK:      (func $f1 (type $i32_=>_none) (param $0 i32)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT: )
 (func $f1 (param i32))
 ;; CHECK:      (func $f2 (type $i32_=>_none) (param $x i32)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT: )
 (func $f2 (param $x i32))
 ;; CHECK:      (func $f3 (type $none_=>_i32) (result i32)
 ;; CHECK-NEXT:  (i32.const 0)
 ;; CHECK-NEXT: )
 (func $f3 (result i32)
  i32.const 0
 )
 ;; CHECK:      (func $f4 (type $void)
 ;; CHECK-NEXT:  (local $0 i32)
 ;; CHECK-NEXT:  (local $1 i64)
 ;; CHECK-NEXT:  (local $l f32)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT: )
 (func $f4 (type 14) (local i32 i64) (local $l f32))
 (func (export "f5.0") (export "f5.1") (import "mod" "f5"))

 ;; CHECK:      (func $nop-skate (type $void)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (unreachable)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT: )
 (func $nop-skate
  nop
  nop
  unreachable
  nop
  nop
 )

 ;; CHECK:      (func $nop-ski (type $void)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (unreachable)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT: )
 (func $nop-ski
  (unreachable
   (nop
    (nop)
    (nop)
    (nop
     (nop)
    )
   )
   (nop)
  )
  (nop)
 )

 ;; CHECK:      (func $nop-sled (type $void)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (unreachable)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (unreachable)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT: )
 (func $nop-sled
  nop
  (nop
   (nop
    (unreachable)
   )
  )
  nop
  (unreachable)
  nop
 )

 ;; CHECK:      (func $add (type $none_=>_i32) (result i32)
 ;; CHECK-NEXT:  (i32.add
 ;; CHECK-NEXT:   (i32.const 1)
 ;; CHECK-NEXT:   (i32.const 2)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $add (result i32)
  i32.const 1
  i32.const 2
  i32.add
 )

 ;; CHECK:      (func $add-folded (type $none_=>_i32) (result i32)
 ;; CHECK-NEXT:  (i32.add
 ;; CHECK-NEXT:   (i32.const 1)
 ;; CHECK-NEXT:   (i32.const 2)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $add-folded (result i32)
  (i32.add
   (i32.const 1)
   (i32.const 2)
  )
 )

 ;; CHECK:      (func $add-stacky (type $none_=>_i32) (result i32)
 ;; CHECK-NEXT:  (local $scratch i32)
 ;; CHECK-NEXT:  (i32.add
 ;; CHECK-NEXT:   (block (result i32)
 ;; CHECK-NEXT:    (local.set $scratch
 ;; CHECK-NEXT:     (i32.const 1)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (nop)
 ;; CHECK-NEXT:    (local.get $scratch)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (i32.const 2)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $add-stacky (result i32)
  i32.const 1
  nop
  i32.const 2
  i32.add
 )

 ;; CHECK:      (func $add-stacky-2 (type $none_=>_i32) (result i32)
 ;; CHECK-NEXT:  (local $scratch i32)
 ;; CHECK-NEXT:  (i32.add
 ;; CHECK-NEXT:   (i32.const 1)
 ;; CHECK-NEXT:   (block (result i32)
 ;; CHECK-NEXT:    (local.set $scratch
 ;; CHECK-NEXT:     (i32.const 2)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (nop)
 ;; CHECK-NEXT:    (local.get $scratch)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $add-stacky-2 (result i32)
  i32.const 1
  i32.const 2
  nop
  i32.add
 )

 ;; CHECK:      (func $add-stacky-3 (type $none_=>_i32) (result i32)
 ;; CHECK-NEXT:  (local $scratch i32)
 ;; CHECK-NEXT:  (local.set $scratch
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:    (i32.const 2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (local.get $scratch)
 ;; CHECK-NEXT: )
 (func $add-stacky-3 (result i32)
  i32.const 1
  i32.const 2
  i32.add
  nop
 )

 ;; CHECK:      (func $add-stacky-4 (type $none_=>_i32) (result i32)
 ;; CHECK-NEXT:  (local $scratch i32)
 ;; CHECK-NEXT:  (local $scratch_0 i32)
 ;; CHECK-NEXT:  (local $scratch_1 i32)
 ;; CHECK-NEXT:  (local.set $scratch_1
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (block (result i32)
 ;; CHECK-NEXT:     (local.set $scratch_0
 ;; CHECK-NEXT:      (i32.const 1)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:     (nop)
 ;; CHECK-NEXT:     (local.get $scratch_0)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (block (result i32)
 ;; CHECK-NEXT:     (local.set $scratch
 ;; CHECK-NEXT:      (i32.const 2)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:     (nop)
 ;; CHECK-NEXT:     (local.get $scratch)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT:  (local.get $scratch_1)
 ;; CHECK-NEXT: )
 (func $add-stacky-4 (result i32)
  i32.const 1
  nop
  i32.const 2
  nop
  i32.add
  nop
 )

 ;; CHECK:      (func $add-unreachable (type $none_=>_i32) (result i32)
 ;; CHECK-NEXT:  (i32.add
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:   (i32.const 1)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $add-unreachable (result i32)
  unreachable
  i32.const 1
  i32.add
 )

 ;; CHECK:      (func $add-unreachable-2 (type $none_=>_i32) (result i32)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (i32.const 1)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (i32.add
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $add-unreachable-2 (result i32)
  i32.const 1
  unreachable
  i32.add
 )

 ;; CHECK:      (func $add-unreachable-3 (type $none_=>_i32) (result i32)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (i32.const 1)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (i32.const 2)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (unreachable)
 ;; CHECK-NEXT: )
 (func $add-unreachable-3 (result i32)
  i32.const 1
  i32.const 2
  unreachable
 )

 ;; CHECK:      (func $add-twice (type $ret2) (result i32 i32)
 ;; CHECK-NEXT:  (tuple.make
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:    (i32.const 2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (i32.const 3)
 ;; CHECK-NEXT:    (i32.const 4)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $add-twice (type $ret2)
  i32.const 1
  i32.const 2
  i32.add
  i32.const 3
  i32.const 4
  i32.add
 )

 ;; CHECK:      (func $add-twice-stacky (type $ret2) (result i32 i32)
 ;; CHECK-NEXT:  (local $scratch i32)
 ;; CHECK-NEXT:  (tuple.make
 ;; CHECK-NEXT:   (block (result i32)
 ;; CHECK-NEXT:    (local.set $scratch
 ;; CHECK-NEXT:     (i32.add
 ;; CHECK-NEXT:      (i32.const 1)
 ;; CHECK-NEXT:      (i32.const 2)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (nop)
 ;; CHECK-NEXT:    (local.get $scratch)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (i32.const 3)
 ;; CHECK-NEXT:    (i32.const 4)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $add-twice-stacky (type $ret2)
  i32.const 1
  i32.const 2
  i32.add
  nop
  i32.const 3
  i32.const 4
  i32.add
 )

 ;; CHECK:      (func $add-twice-stacky-2 (type $ret2) (result i32 i32)
 ;; CHECK-NEXT:  (local $scratch i32)
 ;; CHECK-NEXT:  (tuple.make
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:    (i32.const 2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (block (result i32)
 ;; CHECK-NEXT:    (local.set $scratch
 ;; CHECK-NEXT:     (i32.add
 ;; CHECK-NEXT:      (i32.const 3)
 ;; CHECK-NEXT:      (i32.const 4)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (nop)
 ;; CHECK-NEXT:    (local.get $scratch)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $add-twice-stacky-2 (type $ret2)
  i32.const 1
  i32.const 2
  i32.add
  i32.const 3
  i32.const 4
  i32.add
  nop
 )

 ;; CHECK:      (func $add-twice-unreachable (type $ret2) (result i32 i32)
 ;; CHECK-NEXT:  (tuple.make
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (unreachable)
 ;; CHECK-NEXT:    (i32.const 2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (i32.const 3)
 ;; CHECK-NEXT:    (i32.const 4)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $add-twice-unreachable (type $ret2)
  unreachable
  i32.const 2
  i32.add
  i32.const 3
  i32.const 4
  i32.add
 )

 ;; CHECK:      (func $add-twice-unreachable-2 (type $ret2) (result i32 i32)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:    (i32.const 2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (tuple.make
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (i32.const 3)
 ;; CHECK-NEXT:    (i32.const 4)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $add-twice-unreachable-2 (type $ret2)
  i32.const 1
  i32.const 2
  i32.add
  unreachable
  i32.const 3
  i32.const 4
  i32.add
 )

 ;; CHECK:      (func $add-twice-unreachable-3 (type $ret2) (result i32 i32)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:    (i32.const 2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (i32.const 3)
 ;; CHECK-NEXT:    (i32.const 4)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (tuple.make
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $add-twice-unreachable-3 (type $ret2)
  i32.const 1
  i32.const 2
  i32.add
  i32.const 3
  i32.const 4
  i32.add
  unreachable
 )

 ;; CHECK:      (func $big-stack (type $void)
 ;; CHECK-NEXT:  (local $scratch f64)
 ;; CHECK-NEXT:  (local $scratch_0 i64)
 ;; CHECK-NEXT:  (local $scratch_1 f32)
 ;; CHECK-NEXT:  (local $scratch_2 i32)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (block (result i32)
 ;; CHECK-NEXT:    (local.set $scratch_2
 ;; CHECK-NEXT:     (i32.const 0)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (drop
 ;; CHECK-NEXT:     (block (result f32)
 ;; CHECK-NEXT:      (local.set $scratch_1
 ;; CHECK-NEXT:       (f32.const 1)
 ;; CHECK-NEXT:      )
 ;; CHECK-NEXT:      (drop
 ;; CHECK-NEXT:       (block (result i64)
 ;; CHECK-NEXT:        (local.set $scratch_0
 ;; CHECK-NEXT:         (i64.const 2)
 ;; CHECK-NEXT:        )
 ;; CHECK-NEXT:        (drop
 ;; CHECK-NEXT:         (block (result f64)
 ;; CHECK-NEXT:          (local.set $scratch
 ;; CHECK-NEXT:           (f64.const 3)
 ;; CHECK-NEXT:          )
 ;; CHECK-NEXT:          (drop
 ;; CHECK-NEXT:           (ref.null none)
 ;; CHECK-NEXT:          )
 ;; CHECK-NEXT:          (local.get $scratch)
 ;; CHECK-NEXT:         )
 ;; CHECK-NEXT:        )
 ;; CHECK-NEXT:        (local.get $scratch_0)
 ;; CHECK-NEXT:       )
 ;; CHECK-NEXT:      )
 ;; CHECK-NEXT:      (local.get $scratch_1)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (local.get $scratch_2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $big-stack
  i32.const 0
  f32.const 1
  i64.const 2
  f64.const 3
  ref.null any
  drop
  drop
  drop
  drop
  drop
 )

 ;; CHECK:      (func $locals (type $i32_i32_=>_none) (param $0 i32) (param $x i32)
 ;; CHECK-NEXT:  (local $2 i32)
 ;; CHECK-NEXT:  (local $y i32)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $0)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $2)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $y)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.set $x
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.tee $y
 ;; CHECK-NEXT:    (local.get $y)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $locals (param i32) (param $x i32)
  (local i32)
  (local $y i32)
  local.get 0
  drop
  local.get 1
  drop
  local.get 2
  drop
  local.get 3
  drop
  local.get $x
  local.set 1
  local.get $y
  local.tee 3
  drop
 )


 ;; CHECK:      (func $binary (type $i32_i32_f64_f64_=>_none) (param $0 i32) (param $1 i32) (param $2 f64) (param $3 f64)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (local.get $0)
 ;; CHECK-NEXT:    (local.get $1)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (f64.mul
 ;; CHECK-NEXT:    (local.get $2)
 ;; CHECK-NEXT:    (local.get $3)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $binary (param i32 i32 f64 f64)
  local.get 0
  local.get 1
  i32.add
  drop
  local.get 2
  local.get 3
  f64.mul
  drop
 )

 ;; CHECK:      (func $unary (type $i64_=>_none) (param $0 i64)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (i64.eqz
 ;; CHECK-NEXT:    (local.get $0)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $unary (param i64)
  local.get 0
  i64.eqz
  drop
 )

 ;; CHECK:      (func $select (type $i32_i32_i32_=>_none) (param $0 i32) (param $1 i32) (param $2 i32)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (select
 ;; CHECK-NEXT:    (local.get $0)
 ;; CHECK-NEXT:    (local.get $1)
 ;; CHECK-NEXT:    (local.get $2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (select
 ;; CHECK-NEXT:    (local.get $0)
 ;; CHECK-NEXT:    (local.get $1)
 ;; CHECK-NEXT:    (local.get $2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (select
 ;; CHECK-NEXT:    (local.get $0)
 ;; CHECK-NEXT:    (local.get $1)
 ;; CHECK-NEXT:    (local.get $2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (select
 ;; CHECK-NEXT:    (local.get $0)
 ;; CHECK-NEXT:    (local.get $1)
 ;; CHECK-NEXT:    (local.get $2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $select (param i32 i32 i32)
  local.get 0
  local.get 1
  local.get 2
  select
  drop
  local.get 0
  local.get 1
  local.get 2
  select (result)
  drop
  local.get 0
  local.get 1
  local.get 2
  select (result i32)
  drop
  local.get 0
  local.get 1
  local.get 2
  select (result) (result i32) (result)
  drop
 )

 ;; CHECK:      (func $memory-size (type $void)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (memory.size $mem)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (memory.size $0)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (memory.size $mem-i64)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $memory-size
  memory.size
  drop
  memory.size 1
  drop
  memory.size $mem-i64
  drop
 )

 ;; CHECK:      (func $memory-grow (type $i32_i64_=>_none) (param $0 i32) (param $1 i64)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (memory.grow $mem
 ;; CHECK-NEXT:    (local.get $0)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (memory.grow $0
 ;; CHECK-NEXT:    (local.get $0)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (memory.grow $mem-i64
 ;; CHECK-NEXT:    (local.get $1)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $memory-grow (param i32 i64)
  local.get 0
  memory.grow
  drop
  local.get 0
  memory.grow 1
  drop
  local.get 1
  memory.grow $mem-i64
  drop
 )

 ;; CHECK:      (func $globals (type $void)
 ;; CHECK-NEXT:  (global.set $2
 ;; CHECK-NEXT:   (global.get $i32)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $globals
  global.get $i32
  global.set 4
 )

 ;; CHECK:      (func $simd-extract (type $v128_=>_i32) (param $0 v128) (result i32)
 ;; CHECK-NEXT:  (i32x4.extract_lane 3
 ;; CHECK-NEXT:   (local.get $0)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $simd-extract (param v128) (result i32)
  local.get 0
  i32x4.extract_lane 3
 )

 ;; CHECK:      (func $simd-replace (type $v128_i32_=>_v128) (param $0 v128) (param $1 i32) (result v128)
 ;; CHECK-NEXT:  (i32x4.replace_lane 2
 ;; CHECK-NEXT:   (local.get $0)
 ;; CHECK-NEXT:   (local.get $1)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $simd-replace (param v128 i32) (result v128)
  local.get 0
  local.get 1
  i32x4.replace_lane 2
 )

 ;; CHECK:      (func $simd-shuffle (type $v128_v128_=>_v128) (param $0 v128) (param $1 v128) (result v128)
 ;; CHECK-NEXT:  (i8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23
 ;; CHECK-NEXT:   (local.get $0)
 ;; CHECK-NEXT:   (local.get $1)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $simd-shuffle (param v128 v128) (result v128)
  local.get 0
  local.get 1
  i8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23
 )

 ;; CHECK:      (func $simd-ternary (type $v128_v128_v128_=>_v128) (param $0 v128) (param $1 v128) (param $2 v128) (result v128)
 ;; CHECK-NEXT:  (v128.bitselect
 ;; CHECK-NEXT:   (local.get $0)
 ;; CHECK-NEXT:   (local.get $1)
 ;; CHECK-NEXT:   (local.get $2)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $simd-ternary (param v128 v128 v128) (result v128)
  local.get 0
  local.get 1
  local.get 2
  v128.bitselect
 )

 ;; CHECK:      (func $simd-shift (type $v128_i32_=>_v128) (param $0 v128) (param $1 i32) (result v128)
 ;; CHECK-NEXT:  (i8x16.shl
 ;; CHECK-NEXT:   (local.get $0)
 ;; CHECK-NEXT:   (local.get $1)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $simd-shift (param v128 i32) (result v128)
  local.get 0
  local.get 1
  i8x16.shl
 )

 ;; CHECK:      (func $use-types (type $ref|$s0|_ref|$s1|_ref|$s2|_ref|$s3|_ref|$s4|_ref|$s5|_ref|$s6|_ref|$s7|_ref|$s8|_ref|$a0|_ref|$a1|_ref|$a2|_ref|$a3|_ref|$subvoid|_ref|$submany|_=>_none) (param $0 (ref $s0)) (param $1 (ref $s1)) (param $2 (ref $s2)) (param $3 (ref $s3)) (param $4 (ref $s4)) (param $5 (ref $s5)) (param $6 (ref $s6)) (param $7 (ref $s7)) (param $8 (ref $s8)) (param $9 (ref $a0)) (param $10 (ref $a1)) (param $11 (ref $a2)) (param $12 (ref $a3)) (param $13 (ref $subvoid)) (param $14 (ref $submany))
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT: )
 (func $use-types
  (param (ref $s0))
  (param (ref $s1))
  (param (ref $s2))
  (param (ref $s3))
  (param (ref $s4))
  (param (ref $s5))
  (param (ref $s6))
  (param (ref $s7))
  (param (ref $s8))
  (param (ref $a0))
  (param (ref $a1))
  (param (ref $a2))
  (param (ref $a3))
  (param (ref $subvoid))
  (param (ref $submany))
 )
)
