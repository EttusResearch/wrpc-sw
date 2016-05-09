load snmp_test_config

@test "Get next object after 1.3.6.1.4.1.96.101.1.1.3.0, check if other returned" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1.1.3.0 | grep 1.3.6.1.4.1.96.101.1.1.3.0 | wc -l)"
  [ "$result" -eq 0 ]
}

@test "Get next object after 1.3.6.1.4.1.96.101.1.1.3.0, check if correct returned" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1.1.3.0 | grep 1.3.6.1.4.1.96.101.1.1.4.0 | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Get next object after 1.3.6.1.4.1.95 (first object)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.95 | grep 1.3.6.1.4.1.96.101.1.1.1.0 | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Get next object after the last object (getnext 1.3.6.1.4.1.97)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.97 | wc -l)"
  [ "$result" -eq 0 ]
}

@test "Get next object after oid 1.3.6.1.4.1.96 (oid too short)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96 | grep 1.3.6.1.4.1.96.101.1.1.1.0 | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Get next object after oid 1.3.6.1.4.1.96.0 (oid too short with 0 at the end)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.0 | grep 1.3.6.1.4.1.96.101.1.1.1.0 | wc -l)"
  [ "$result" -eq 1 ]
}


@test "Get next object after oid 1.3.6.1.4.1.96.101 (oid too short)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101 | grep 1.3.6.1.4.1.96.101.1.1.1.0 | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Get next object after oid 1.3.6.1.4.1.96.101.1 (oid too short)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1 | grep 1.3.6.1.4.1.96.101.1.1.1.0 | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Get next object after oid 1.3.6.1.4.1.96.101.1.1 (oid too short)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1.1 | grep 1.3.6.1.4.1.96.101.1.1.1.0 | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Get next object after oid 1.3.6.1.4.1.96.101.1.1.1 (missing 0 at the end)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1.1.1 | grep 1.3.6.1.4.1.96.101.1.1.1.0 | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Get next object after oid 1.3.6.1.4.1.96.101.1.1.2 (missing 0 at the end)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1.1.2 | grep 1.3.6.1.4.1.96.101.1.1.2.0 | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Get next object after oid 1.3.6.1.4.1.96.101.1.1.1.0 (jump to the next oid in the branch)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1.1.1.0 | grep 1.3.6.1.4.1.96.101.1.1.2.0 | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Get next object after oid 1.3.6.1.4.1.96.101.1.1.1.1 (too deep in the branch)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1.1.1.1 | grep 1.3.6.1.4.1.96.101.1.1.2.0 | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Get next object after oid 1.3.6.1.4.1.96.101.1.1.1.1.1 (too deep in the branch)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1.1.1.1.1 | grep 1.3.6.1.4.1.96.101.1.1.2.0 | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Get next object after oid 1.3.6.1.4.1.96.101.1.1.1.1.1.0 (too deep in the branch)" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1.1.1.1.1.0 | grep 1.3.6.1.4.1.96.101.1.1.2.0 | wc -l)"
  [ "$result" -eq 1 ]
}
