(module
  (type $X (struct (field (mut i32))))

  (tag $tag)

  (export "branch" (func $branch))

  (export "throw" (func $throw))

  (func $branch (result i32)
    (local $x (ref null $X))
    (block $out
      (struct.set $X 0
        (local.tee $x
          (struct.new $X
            (i32.const 1) ;; the struct's field begins at 1
          )
        )
        ;; This if will set the field to 0, but *not* if it branches. If we
        ;; merged the struct.set with the tee and the new then we would break
        ;; things.
        (if (result i32)
          (i32.const 1)
          (then
            (br $out)
          )
          (else
            (i32.const 0)
          )
        )
      )
    )
    (struct.get $X 0
      (local.get $x)
    )
  )

  (func $throw (result i32)
    (local $x (ref null $X))
    (try
      (do
        (struct.set $X 0
          (local.tee $x
            (struct.new $X
              (i32.const 1) ;; the struct's field begins at 1
            )
          )
          ;; This if will set the field to 0, but *not* if it throws. If we
          ;; merged the struct.set with the tee and the new then we would break
          ;; things.
          (if (result i32)
            (i32.const 1)
            (then
              (throw $tag)
            )
            (else
              (i32.const 0)
            )
          )
        )
      )
      (catch $tag)
    )
    (struct.get $X 0
      (local.get $x)
    )
  )
)
