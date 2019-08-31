#!/usr/bin/env bats

teardown() {
  rm -f runlock-test
}

@test "runlock: no lock" {
  TS="$(date --date="yesterday" +%s)"
  rm -f runlock-test
  run runlock --timestamp="$TS" -f runlock-test true
cat << EOF
$status
$output
EOF
  [ "$status" -eq 0 ]

  run runlock --timestamp="$TS" -f runlock-test true
cat << EOF
$status
$output
EOF
  [ "$status" -eq 121 ]
}

@test "runlock: runnable timestamp" {
  TS="$(date --date="yesterday" +%s)"
  touch --date="last year" runlock-test
  run runlock --timestamp="$TS" -f runlock-test true
cat << EOF
$status
$output
EOF
  [ "$status" -eq 0 ]

  run runlock --timestamp="$TS" -f runlock-test true
cat << EOF
$status
$output
EOF
  [ "$status" -eq 121 ]
}

@test "runlock: subprocess exits non-0" {
  TS="$(date --date="yesterday" +%s)"
  rm -f runlock-test
  run runlock --timestamp="$TS" -f runlock-test false
cat << EOF
$status
$output
EOF
  [ "$status" -eq 1 ]

  run runlock --timestamp="$TS" -f runlock-test true
cat << EOF
$status
$output
EOF
  [ "$status" -eq 0 ]
  [ -f "runlock-test" ]
}

@test "runlock: signals" {
  TS="$(date --date="yesterday" +%s)"
  rm -f runlock-test
  (sleep 2; pkill -TERM runlock) &
  run runlock --timestamp="$TS" -f runlock-test sleep 900
cat << EOF
$status
$output
EOF
  [ "$status" -eq 143 ]
}

@test "runlock: timeout" {
  TS="$(date --date="yesterday" +%s)"
  rm -f runlock-test
  run runlock --timeout=1 --timestamp="$TS" -f runlock-test sleep 900
cat << EOF
$status
$output
EOF
  [ "$status" -eq 143 ]
}
