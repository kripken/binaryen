;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.
;; RUN: wasm-opt %s --tuple-optimization -all -S -o - | filecheck %s

(module
  ;; CHECK:      (func $just-set (type $0)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $1 i32)
  ;; CHECK-NEXT:  (local $2 i32)
  ;; CHECK-NEXT:  (local.set $1
  ;; CHECK-NEXT:   (i32.const 1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $2
  ;; CHECK-NEXT:   (i32.const 2)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $just-set
    (local $tuple (i32 i32))
    ;; This tuple local can be optimized into separate locals per lane. The
    ;; tuple local itself then has no uses and other passes will remove it.
    (local.set $tuple
      (tuple.make
        (i32.const 1)
        (i32.const 2)
      )
    )
  )

  ;; CHECK:      (func $just-get (type $0)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $1 i32)
  ;; CHECK-NEXT:  (local $2 i32)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $2)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $just-get
    (local $tuple (i32 i32))
    ;; The default value of the tuple lanes is used here in the new locals we
    ;; add.
    (drop
      (tuple.extract 0
        (local.get $tuple)
      )
    )
    (drop
      (tuple.extract 1
        (local.get $tuple)
      )
    )
  )

  ;; CHECK:      (func $just-get-bad (type $1) (result i32 i32)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (tuple.extract 0
  ;; CHECK-NEXT:    (local.get $tuple)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (tuple.extract 1
  ;; CHECK-NEXT:    (local.get $tuple)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.get $tuple)
  ;; CHECK-NEXT: )
  (func $just-get-bad (result i32 i32)
    (local $tuple (i32 i32))
    (drop
      (tuple.extract 0
        (local.get $tuple)
      )
    )
    (drop
      (tuple.extract 1
        (local.get $tuple)
      )
    )
    ;; This get is not used by something we can handle (it escapes from the
    ;; function), so we should not try to optimize this tuple.
    (local.get $tuple)
  )

  ;; CHECK:      (func $set-and-gets (type $0)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $1 i32)
  ;; CHECK-NEXT:  (local $2 i32)
  ;; CHECK-NEXT:  (block
  ;; CHECK-NEXT:   (local.set $1
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $2
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $2)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $set-and-gets
    (local $tuple (i32 i32))
    (local.set $tuple
      (tuple.make
        (i32.const 1)
        (i32.const 2)
      )
    )
    (drop
      (tuple.extract 0
        (local.get $tuple)
      )
    )
    (drop
      (tuple.extract 1
        (local.get $tuple)
      )
    )
    ;; Add another get for more coverage
    (drop
      (tuple.extract 0
        (local.get $tuple)
      )
    )
  )

  ;; CHECK:      (func $tee (type $0)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $tuple2 (i32 i32))
  ;; CHECK-NEXT:  (local $2 i32)
  ;; CHECK-NEXT:  (local $3 i32)
  ;; CHECK-NEXT:  (local $4 i32)
  ;; CHECK-NEXT:  (local $5 i32)
  ;; CHECK-NEXT:  (block
  ;; CHECK-NEXT:   (block
  ;; CHECK-NEXT:    (local.set $4
  ;; CHECK-NEXT:     (i32.const 1)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (local.set $5
  ;; CHECK-NEXT:     (i32.const 2)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $2
  ;; CHECK-NEXT:    (local.get $4)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $3
  ;; CHECK-NEXT:    (local.get $5)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $2)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $3)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $4)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $5)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $tee
    (local $tuple (i32 i32))
    (local $tuple2 (i32 i32))
    (local.set $tuple
      (local.tee $tuple2
        (tuple.make
          (i32.const 1)
          (i32.const 2)
        )
      )
    )
    ;; Read the first tuple.
    (drop
      (tuple.extract 0
        (local.get $tuple)
      )
    )
    (drop
      (tuple.extract 1
        (local.get $tuple)
      )
    )
    ;; Read the second tuple.
    (drop
      (tuple.extract 0
        (local.get $tuple2)
      )
    )
    (drop
      (tuple.extract 1
        (local.get $tuple2)
      )
    )
  )

  ;; CHECK:      (func $just-tee (type $0)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $1 i32)
  ;; CHECK-NEXT:  (local $2 i32)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (block (result i32)
  ;; CHECK-NEXT:    (block
  ;; CHECK-NEXT:     (local.set $1
  ;; CHECK-NEXT:      (i32.const 1)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:     (local.set $2
  ;; CHECK-NEXT:      (i32.const 2)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (local.get $1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $just-tee
    (local $tuple (i32 i32))
    (drop
      (tuple.extract 0
        (local.tee $tuple
          (tuple.make
            (i32.const 1)
            (i32.const 2)
          )
        )
      )
    )
  )

  ;; CHECK:      (func $just-tee-bad (type $1) (result i32 i32)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local.tee $tuple
  ;; CHECK-NEXT:   (tuple.make
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $just-tee-bad (result i32 i32)
    (local $tuple (i32 i32))
    ;; This tee goes somewhere we cannot handle, so we do not optimize here.
    (local.tee $tuple
      (tuple.make
        (i32.const 1)
        (i32.const 2)
      )
    )
  )

  ;; CHECK:      (func $no-uses (type $0)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $tuple2 (i32 i32))
  ;; CHECK-NEXT:  (local $2 i32)
  ;; CHECK-NEXT:  (local $3 i32)
  ;; CHECK-NEXT:  (local $4 i32)
  ;; CHECK-NEXT:  (local $5 i32)
  ;; CHECK-NEXT:  (block
  ;; CHECK-NEXT:   (local.set $4
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $5
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $2
  ;; CHECK-NEXT:   (local.get $4)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $3
  ;; CHECK-NEXT:   (local.get $5)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $no-uses
    (local $tuple (i32 i32))
    (local $tuple2 (i32 i32))
    ;; The set has no uses, and the tee only has an immediate use. We can
    ;; still optimize both.
    (local.set $tuple
      (local.tee $tuple2
        (tuple.make
          (i32.const 1)
          (i32.const 2)
        )
      )
    )
  )

  ;; CHECK:      (func $corruption-tee (type $1) (result i32 i32)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $tuple2 (i32 i32))
  ;; CHECK-NEXT:  (local.set $tuple
  ;; CHECK-NEXT:   (local.tee $tuple2
  ;; CHECK-NEXT:    (tuple.make
  ;; CHECK-NEXT:     (i32.const 1)
  ;; CHECK-NEXT:     (i32.const 2)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.get $tuple2)
  ;; CHECK-NEXT: )
  (func $corruption-tee (result i32 i32)
    (local $tuple (i32 i32))
    (local $tuple2 (i32 i32))
    ;; As above, but the tee's tuple is bad and it prevents the other from
    ;; being optimized too, due to the copy between them.
    (local.set $tuple
      (local.tee $tuple2
        (tuple.make
          (i32.const 1)
          (i32.const 2)
        )
      )
    )
    (local.get $tuple2)
  )

  ;; CHECK:      (func $corruption-set (type $1) (result i32 i32)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $tuple2 (i32 i32))
  ;; CHECK-NEXT:  (local.set $tuple
  ;; CHECK-NEXT:   (local.tee $tuple2
  ;; CHECK-NEXT:    (tuple.make
  ;; CHECK-NEXT:     (i32.const 1)
  ;; CHECK-NEXT:     (i32.const 2)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.get $tuple)
  ;; CHECK-NEXT: )
  (func $corruption-set (result i32 i32)
    (local $tuple (i32 i32))
    (local $tuple2 (i32 i32))
    ;; As above, but now the set is bad.
    (local.set $tuple
      (local.tee $tuple2
        (tuple.make
          (i32.const 1)
          (i32.const 2)
        )
      )
    )
    (local.get $tuple) ;; this changed from $tuple2; same outcome.
  )

  ;; CHECK:      (func $set-after-set (type $0)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $tuple2 (i32 i32))
  ;; CHECK-NEXT:  (local $2 i32)
  ;; CHECK-NEXT:  (local $3 i32)
  ;; CHECK-NEXT:  (local $4 i32)
  ;; CHECK-NEXT:  (local $5 i32)
  ;; CHECK-NEXT:  (block
  ;; CHECK-NEXT:   (local.set $2
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $3
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (block
  ;; CHECK-NEXT:   (local.set $4
  ;; CHECK-NEXT:    (local.get $2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $5
  ;; CHECK-NEXT:    (local.get $3)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (block
  ;; CHECK-NEXT:   (local.set $2
  ;; CHECK-NEXT:    (local.get $2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $3
  ;; CHECK-NEXT:    (local.get $3)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $set-after-set
    (local $tuple (i32 i32))
    (local $tuple2 (i32 i32))
    ;; We can optimize both these tuples.
    (local.set $tuple
      (tuple.make
        (i32.const 1)
        (i32.const 2)
      )
    )
    (local.set $tuple2
      (local.get $tuple)
    )
    ;; Add a copy of a tuple to itself for extra coverage.
    (local.set $tuple
      (local.get $tuple)
    )
  )

  ;; CHECK:      (func $corruption-first-set (type $1) (result i32 i32)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $tuple2 (i32 i32))
  ;; CHECK-NEXT:  (local.set $tuple
  ;; CHECK-NEXT:   (tuple.make
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $tuple2
  ;; CHECK-NEXT:   (local.get $tuple)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.get $tuple)
  ;; CHECK-NEXT: )
  (func $corruption-first-set (result i32 i32)
    (local $tuple (i32 i32))
    (local $tuple2 (i32 i32))
    (local.set $tuple
      (tuple.make
        (i32.const 1)
        (i32.const 2)
      )
    )
    (local.set $tuple2
      (local.get $tuple)
    )
    ;; This local.get prevents both locals from being optimized.
    (local.get $tuple)
  )

  ;; CHECK:      (func $corruption-second-set (type $1) (result i32 i32)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $tuple2 (i32 i32))
  ;; CHECK-NEXT:  (local.set $tuple
  ;; CHECK-NEXT:   (tuple.make
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $tuple2
  ;; CHECK-NEXT:   (local.get $tuple)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.get $tuple2)
  ;; CHECK-NEXT: )
  (func $corruption-second-set (result i32 i32)
    (local $tuple (i32 i32))
    (local $tuple2 (i32 i32))
    (local.set $tuple
      (tuple.make
        (i32.const 1)
        (i32.const 2)
      )
    )
    (local.set $tuple2
      (local.get $tuple)
    )
    (local.get $tuple2) ;; this changed from $tuple; same outcome.
  )

  ;; CHECK:      (func $other (type $0)
  ;; CHECK-NEXT:  (local $other f64)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local.set $other
  ;; CHECK-NEXT:   (local.tee $other
  ;; CHECK-NEXT:    (local.get $other)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $other
    ;; A non-tuple local and all operations on it should be ignored.
    (local $other f64)
    ;; A tuple local with no uses at all should be ignored.
    (local $tuple (i32 i32))
    (local.set $other
      (local.tee $other
        (local.get $other)
      )
    )
  )

  ;; CHECK:      (func $make-extract-no-local (type $0)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (tuple.extract 0
  ;; CHECK-NEXT:    (tuple.make
  ;; CHECK-NEXT:     (i32.const 1)
  ;; CHECK-NEXT:     (i32.const 2)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $make-extract-no-local
    ;; Tuple operations without locals. We do nothing here; other passes can
    ;; help on this kind of thing.
    (drop
      (tuple.extract 0
        (tuple.make
          (i32.const 1)
          (i32.const 2)
        )
      )
    )
  )

  ;; CHECK:      (func $make-extract-no-local-but-other (type $0)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $1 i32)
  ;; CHECK-NEXT:  (local $2 i32)
  ;; CHECK-NEXT:  (block
  ;; CHECK-NEXT:   (local.set $1
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $2
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (tuple.extract 0
  ;; CHECK-NEXT:    (tuple.make
  ;; CHECK-NEXT:     (i32.const 1)
  ;; CHECK-NEXT:     (i32.const 2)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $make-extract-no-local-but-other
    (local $tuple (i32 i32))
    (local.set $tuple
      (tuple.make
        (i32.const 1)
        (i32.const 2)
      )
    )
    ;; The code below is as in the previous testcase, but now before us there
    ;; is an unrelated local that can be optimized. We should remain as before.
    (drop
      (tuple.extract 0
        (tuple.make
          (i32.const 1)
          (i32.const 2)
        )
      )
    )
  )

  ;; CHECK:      (func $set-of-block (type $0)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local.set $tuple
  ;; CHECK-NEXT:   (block (result i32 i32)
  ;; CHECK-NEXT:    (tuple.make
  ;; CHECK-NEXT:     (i32.const 1)
  ;; CHECK-NEXT:     (i32.const 2)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $set-of-block
    (local $tuple (i32 i32))
    ;; We do not handle blocks yet, so this is not optimized.
    (local.set $tuple
      (block (result i32 i32)
        (tuple.make
          (i32.const 1)
          (i32.const 2)
        )
      )
    )
  )

  ;; CHECK:      (func $unreachability (type $0)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local.set $tuple
  ;; CHECK-NEXT:   (tuple.make
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.tee $tuple
  ;; CHECK-NEXT:   (unreachable)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (tuple.extract 0
  ;; CHECK-NEXT:    (unreachable)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (tuple.extract 1
  ;; CHECK-NEXT:    (local.tee $tuple
  ;; CHECK-NEXT:     (unreachable)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $unreachability
    (local $tuple (i32 i32))
    (local.set $tuple
      (tuple.make
        (i32.const 1)
        (i32.const 2)
      )
    )
    ;; We should not error here, and do nothing.
    (local.set $tuple
      (unreachable)
    )
    (drop
      (tuple.extract 0
        (unreachable)
      )
    )
    (drop
      (tuple.extract 1
        (local.tee $tuple
          (unreachable)
        )
      )
    )
  )

  ;; CHECK:      (func $tee-chain (type $0)
  ;; CHECK-NEXT:  (local $tuple (i32 i32))
  ;; CHECK-NEXT:  (local $tuple2 (i32 i32))
  ;; CHECK-NEXT:  (local $tuple3 (i32 i32))
  ;; CHECK-NEXT:  (local $3 i32)
  ;; CHECK-NEXT:  (local $4 i32)
  ;; CHECK-NEXT:  (local $5 i32)
  ;; CHECK-NEXT:  (local $6 i32)
  ;; CHECK-NEXT:  (local $7 i32)
  ;; CHECK-NEXT:  (local $8 i32)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (block (result i32)
  ;; CHECK-NEXT:    (block
  ;; CHECK-NEXT:     (block
  ;; CHECK-NEXT:      (block
  ;; CHECK-NEXT:       (local.set $7
  ;; CHECK-NEXT:        (i32.const 1)
  ;; CHECK-NEXT:       )
  ;; CHECK-NEXT:       (local.set $8
  ;; CHECK-NEXT:        (i32.const 2)
  ;; CHECK-NEXT:       )
  ;; CHECK-NEXT:      )
  ;; CHECK-NEXT:      (local.set $5
  ;; CHECK-NEXT:       (local.get $7)
  ;; CHECK-NEXT:      )
  ;; CHECK-NEXT:      (local.set $6
  ;; CHECK-NEXT:       (local.get $8)
  ;; CHECK-NEXT:      )
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:     (local.set $3
  ;; CHECK-NEXT:      (local.get $5)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:     (local.set $4
  ;; CHECK-NEXT:      (local.get $6)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (local.get $3)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $tee-chain
    (local $tuple (i32 i32))
    (local $tuple2 (i32 i32))
    (local $tuple3 (i32 i32))
    (drop
      (tuple.extract 0
        (local.tee $tuple
          (local.tee $tuple2
            (local.tee $tuple3
              (tuple.make
                (i32.const 1)
                (i32.const 2)
              )
            )
          )
        )
      )
    )
  )

  ;; CHECK:      (func $chain-3 (type $0)
  ;; CHECK-NEXT:  (local $tuple (i32 i32 i32))
  ;; CHECK-NEXT:  (local $tuple2 (i32 i32 i32))
  ;; CHECK-NEXT:  (local $tuple3 (i32 i32 i32))
  ;; CHECK-NEXT:  (local $3 i32)
  ;; CHECK-NEXT:  (local $4 i32)
  ;; CHECK-NEXT:  (local $5 i32)
  ;; CHECK-NEXT:  (local $6 i32)
  ;; CHECK-NEXT:  (local $7 i32)
  ;; CHECK-NEXT:  (local $8 i32)
  ;; CHECK-NEXT:  (local $9 i32)
  ;; CHECK-NEXT:  (local $10 i32)
  ;; CHECK-NEXT:  (local $11 i32)
  ;; CHECK-NEXT:  (block
  ;; CHECK-NEXT:   (local.set $3
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $4
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $5
  ;; CHECK-NEXT:    (i32.const 3)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (block
  ;; CHECK-NEXT:   (local.set $6
  ;; CHECK-NEXT:    (local.get $3)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $7
  ;; CHECK-NEXT:    (local.get $4)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $8
  ;; CHECK-NEXT:    (local.get $5)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (block
  ;; CHECK-NEXT:   (local.set $9
  ;; CHECK-NEXT:    (local.get $6)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $10
  ;; CHECK-NEXT:    (local.get $7)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $11
  ;; CHECK-NEXT:    (local.get $8)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $3)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $7)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $11)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $chain-3
    (local $tuple (i32 i32 i32))
    (local $tuple2 (i32 i32 i32))
    (local $tuple3 (i32 i32 i32))
    ;; A chain of 3 copied tuples.
    (local.set $tuple
      (tuple.make
        (i32.const 1)
        (i32.const 2)
        (i32.const 3)
      )
    )
    (local.set $tuple2
      (local.get $tuple)
    )
    (local.set $tuple3
      (local.get $tuple2)
    )
    ;; Read from each.
    (drop
      (tuple.extract 0
        (local.get $tuple)
      )
    )
    (drop
      (tuple.extract 1
        (local.get $tuple2)
      )
    )
    (drop
      (tuple.extract 2
        (local.get $tuple3)
      )
    )
  )

  ;; CHECK:      (func $chain-3-corruption (type $2) (result i32 i32 i32)
  ;; CHECK-NEXT:  (local $tuple (i32 i32 i32))
  ;; CHECK-NEXT:  (local $tuple2 (i32 i32 i32))
  ;; CHECK-NEXT:  (local $tuple3 (i32 i32 i32))
  ;; CHECK-NEXT:  (local.set $tuple
  ;; CHECK-NEXT:   (tuple.make
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:    (i32.const 3)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $tuple2
  ;; CHECK-NEXT:   (local.get $tuple)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $tuple3
  ;; CHECK-NEXT:   (local.get $tuple2)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (tuple.extract 0
  ;; CHECK-NEXT:    (local.get $tuple)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (tuple.extract 1
  ;; CHECK-NEXT:    (local.get $tuple2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (tuple.extract 2
  ;; CHECK-NEXT:    (local.get $tuple3)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.get $tuple)
  ;; CHECK-NEXT: )
  (func $chain-3-corruption (result i32 i32 i32)
    (local $tuple (i32 i32 i32))
    (local $tuple2 (i32 i32 i32))
    (local $tuple3 (i32 i32 i32))
    ;; As above, but a get at the very end prevents the entire chain from being
    ;; optimized.
    (local.set $tuple
      (tuple.make
        (i32.const 1)
        (i32.const 2)
        (i32.const 3)
      )
    )
    (local.set $tuple2
      (local.get $tuple)
    )
    (local.set $tuple3
      (local.get $tuple2)
    )
    ;; Read from each.
    (drop
      (tuple.extract 0
        (local.get $tuple)
      )
    )
    (drop
      (tuple.extract 1
        (local.get $tuple2)
      )
    )
    (drop
      (tuple.extract 2
        (local.get $tuple3)
      )
    )
    (local.get $tuple) ;; this was added
  )

  (func $set-call (result i32 i32)
    (local $tuple (i32 i32))
    ;; Setting from a call prevents optimization.
    (local.set $tuple
      (call $set-call)
    )
    (drop
      (tuple.extract 0
        (local.get $tuple)
      )
    )
)
