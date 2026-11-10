import Lake
open Lake DSL

package alarmack_companion

lean_lib AlarmAck where
  roots := #[`Common, `Parser, `Recovery]

@[test_driver]
script test (args) do
  let child ← IO.Process.spawn {
    cmd := "node"
    args := #["verify.mjs"] ++ args.toArray
  }
  child.wait
