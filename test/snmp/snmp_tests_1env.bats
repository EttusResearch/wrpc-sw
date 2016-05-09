load snmp_test_config

@test "Host up $TARGET_IP" {
  if [ x"$TARGET_IP" = "x" ]; then
    echo "no TARGET_IP set"
  fi
  # to know if out target system is up
  run ping -c 1 $TARGET_IP -W 2
  [ "$status" -eq 0 ]
}

@test "Check the presence of snmpget" {
  # to know if we have snmpget available
  run command -v snmpget
  [ "$status" -eq 0 ]
}

@test "Check the presence of snmpgetnext" {
  # to know if we have snmpgetnext available
  run command -v snmpgetnext
  [ "$status" -eq 0 ]
}

@test "Check the presence of snmpwalk" {
  # to know if we have snmpwalk available
  run command -v snmpwalk
  [ "$status" -eq 0 ]
}

@test "Check the presence of snmpset" {
  # to know if we have snmpset available
  run command -v snmpset
  [ "$status" -eq 0 ]
}
