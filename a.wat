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
        ;; This branch skips the struct.set, but the struct.new and tee should
        ;; have executed. (The typed block prevents it from being trivially
        ;; eliminated as unreachable code; we could also have an if here that
        ;; only branches some of the time, etc.)
        (block (result i32)
          (br $out)
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
          ;; This throw skips the struct.set, but the struct.new and tee should
          ;; have executed.
          (block (result i32)
            (throw $tag)
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
