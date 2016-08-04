load snmp_test_config
load snmp_test_helpers

@test "Get index of a table 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1, check if correct returned" {
  helper_snmpget_oidfound 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1
}

@test "Get number of aux RO registers of a table 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1, check if correct returned" {
  helper_snmpget_oidfound 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1
}

@test "Get invalid OID 1.3.6.1.4.1.96.101.2, check if correct returned" {
  helper_snmpget_oidnotfound 1.3.6.1.4.1.96.101.2
}

@test "Get invalid OID 1.3.6.1.4.1.96.101.2.1, check if correct returned" {
  helper_snmpget_oidnotfound 1.3.6.1.4.1.96.101.2.1
}

@test "Get invalid OID 1.3.6.1.4.1.96.101.2.1.2, check if correct returned" {
  helper_snmpget_oidnotfound 1.3.6.1.4.1.96.101.2.1.2
}

@test "Get invalid OID 1.3.6.1.4.1.96.101.2.1.2.1, check if correct returned" {
  helper_snmpget_oidnotfound 1.3.6.1.4.1.96.101.2.1.2.1
}

@test "Get invalid OID 1.3.6.1.4.1.96.101.2.1.2.1.1, check if correct returned" {
  helper_snmpget_oidnotfound 1.3.6.1.4.1.96.101.2.1.2.1.1
}

@test "Get invalid OID 1.3.6.1.4.1.96.101.2.1.2.1.1.3, check if correct returned" {
  helper_snmpget_oidnotfound 1.3.6.1.4.1.96.101.2.1.2.1.1.3
}

@test "Get invalid OID 1.3.6.1.4.1.96.101.2.1.2.1.1.3.0, check if correct returned" {
  helper_snmpget_oidnotfound 1.3.6.1.4.1.96.101.2.1.2.1.1.3.0
}

@test "Get number of aux RO registers of a table 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1, check if correct returned" {
  helper_snmpget_oidfound 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1
}

@test "Get invalid OID 1.3.6.1.4.1.96.101.2.1.2.1.1.3.2, check if correct returned" {
  helper_snmpget_oidnotfound 1.3.6.1.4.1.96.101.2.1.2.1.1.3.2
}


@test "Get invalid OID 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1.1, check if correct returned" {
  helper_snmpget_oidnotfound 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1.1
}

# get next


@test "Get next object after 1.3.6.1.4.1.96.101.2, check if correct returned" {
  helper_snmpgetnext 1.3.6.1.4.1.96.101.2 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1
}

@test "Get next object after 1.3.6.1.4.1.96.101.2.1, check if correct returned" {
  helper_snmpgetnext 1.3.6.1.4.1.96.101.2.1 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1
}

@test "Get next object after 1.3.6.1.4.1.96.101.2.1.2, check if correct returned" {
  helper_snmpgetnext 1.3.6.1.4.1.96.101.2.1.2 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1
}

@test "Get next object after 1.3.6.1.4.1.96.101.2.1.2.1, check if correct returned" {
  helper_snmpgetnext 1.3.6.1.4.1.96.101.2.1.2.1 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1
}

@test "Get next object after 1.3.6.1.4.1.96.101.2.1.2.1.1, check if correct returned" {
  helper_snmpgetnext 1.3.6.1.4.1.96.101.2.1.2.1.1 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1
}

@test "Get next object after 1.3.6.1.4.1.96.101.2.1.2.1.1.2, check if correct returned" {
  helper_snmpgetnext 1.3.6.1.4.1.96.101.2.1.2.1.1.2 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1
}

@test "Get next object after 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1, check if other returned" {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1 | grep 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1 | wc -l)"
  [ "$result" -eq 0 ]
}

@test "Get next object after 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1, check if correct returned" {
  helper_snmpgetnext 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1
}

@test "Get next object after 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1, check if correct returned" {
  helper_snmpgetnext 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1 1.3.6.1.4.1.96.101.2.1.2.1.1.4.1
}

@test "Get next object after 1.3.6.1.4.1.96.101.2.1.2.1.1.2.0, check if correct returned" {
  helper_snmpgetnext 1.3.6.1.4.1.96.101.2.1.2.1.1.2.0 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1
}

@test "Get next object after 1.3.6.1.4.1.96.101.2.1.2.1.1.2.0.1, check if correct returned" {
  helper_snmpgetnext 1.3.6.1.4.1.96.101.2.1.2.1.1.2.0.1 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1
}

@test "Get next object after 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1.0, check if correct returned" {
  helper_snmpgetnext 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1.0 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1
}

@test "Get next object after 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1.1, check if correct returned" {
  helper_snmpgetnext 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1.1 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1
}



# sets
@test "Set read-only object 1.3.6.1.4.1.96.101.2.1.2.1.1.1.1" {
  helper_snmpset_failed_u 1.3.6.1.4.1.96.101.2.1.2.1.1.1.1 99
}

@test "Set read-only object 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1" {
  helper_snmpset_failed_u 1.3.6.1.4.1.96.101.2.1.2.1.1.2.1 99
}

@test "Set read-only object 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1" {
  helper_snmpset_failed_u 1.3.6.1.4.1.96.101.2.1.2.1.1.3.1 99
}

@test "Set read-only object 1.3.6.1.4.1.96.101.2.1.2.2.1.1.1" {
  helper_snmpset_failed_u 1.3.6.1.4.1.96.101.2.1.2.2.1.1.1 99
}

@test "Set read-only object 1.3.6.1.4.1.96.101.2.1.2.2.1.2.1" {
  helper_snmpset_failed_u 1.3.6.1.4.1.96.101.2.1.2.2.1.2.1 99
}

@test "Set read-write object 1.3.6.1.4.1.96.101.2.1.2.2.1.3.1, check if correct set" {
  helper_snmpset_u 1.3.6.1.4.1.96.101.2.1.2.2.1.3.1 99
}

@test "Get read-write object 1.3.6.1.4.1.96.101.2.1.2.2.1.3.1" {
  helper_snmpget 1.3.6.1.4.1.96.101.2.1.2.2.1.3.1 99
}

