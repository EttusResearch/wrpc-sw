load snmp_test_config
load snmp_test_helpers

@test "snmpget existing oid 1.3.6.1.4.1.96.101.1.1.1.0" {
  helper_snmpget 1.3.6.1.4.1.96.101.1.1.1.0 1.3.6.1.4.1.96.101.1.1.1.0
}

@test "snmpget existing oid 1.3.6.1.4.1.96.101.1.1.2.0" {
  helper_snmpget 1.3.6.1.4.1.96.101.1.1.2.0 1.3.6.1.4.1.96.101.1.1.2.0
  [ "$result" -eq 1 ]
}

@test "snmpget non existing oid 1.3.6.1.4.1.96.101.1.1.2" {
  result="$(snmpget $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1.1.2 | grep 1.3.6.1.4.1.96.101.1.1.2 | wc -l)"
  [ "$result" -eq 0 ]
}

@test "snmpget non existing oid 1.3.6.1.4.1.96.101.1.1" {
  result="$(snmpget $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1.1 | grep 1.3.6.1.4.1.96.101.1.1 | wc -l)"
  [ "$result" -eq 0 ]
}

@test "snmpget non existing oid 1.3.6.1.4.1.96.101.1" {
  result="$(snmpget $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1 | grep 1.3.6.1.4.1.96.101.1 | wc -l)"
  [ "$result" -eq 0 ]
}

@test "snmpget non existing oid 1.3.6.1.4.1.96.101" {
  result="$(snmpget $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1 | grep 1.3.6.1.4.1.96.101.1 | wc -l)"
  [ "$result" -eq 0 ]
}

@test "snmpget non existing oid 1.3.6.1.4.1.96" {
  result="$(snmpget $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96 | grep 1.3.6.1.4.1.96 | wc -l)"
  [ "$result" -eq 0 ]
}

@test "snmpget non existing oid 1.3.6.1.4.1" {
  result="$(snmpget $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1 | grep 1.3.6.1.4.1 | wc -l)"
  [ "$result" -eq 0 ]
}
