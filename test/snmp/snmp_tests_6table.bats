load snmp_test_config
load snmp_test_helpers

@test "Get wrpcTemperatureTable" {
  helper_snmpget_oidnotfound .1.3.6.1.4.1.96.101.1.3
}

@test "Get wrpcTemperatureTable.0" {
  helper_snmpget_oidnotfound .1.3.6.1.4.1.96.101.1.3.0
}

@test "Get wrpcTemperatureTable.1" {
  helper_snmpget_oidnotfound .1.3.6.1.4.1.96.101.1.3.1
}

@test "Get wrpcTemperatureTable.1.1" {
  helper_snmpget_oidnotfound .1.3.6.1.4.1.96.101.1.3.1.1
}

@test "Get wrpcTemperatureTable.1.1.1" {
  helper_snmpget_oidnotfound .1.3.6.1.4.1.96.101.1.3.1.1.1
}

@test "Get wrpcTemperatureTable.1.1.1.1" {
  helper_snmpget_oidnotfound .1.3.6.1.4.1.96.101.1.3.1.1.1.1
}

@test "Get wrpcTemperatureTable.1.2" {
  helper_snmpget_oidnotfound .1.3.6.1.4.1.96.101.1.3.1.2
}

@test "Get wrpcTemperatureTable.1.2.1" {
  helper_snmpget_oidfound .1.3.6.1.4.1.96.101.1.3.1.2.1
}

@test "Get wrpcTemperatureTable.1.2.1.1" {
  helper_snmpget_oidnotfound .1.3.6.1.4.1.96.101.1.3.1.2.1.1
}

@test "Get wrpcTemperatureTable.1.2.1.1.1" {
  helper_snmpget_oidnotfound .1.3.6.1.4.1.96.101.1.3.1.2.1.1.1
}

@test "Get wrpcTemperatureTable.1.3" {
  helper_snmpget_oidnotfound .1.3.6.1.4.1.96.101.1.3.1.3
}

@test "Get wrpcTemperatureTable.1.3.1" {
  helper_snmpget_oidfound .1.3.6.1.4.1.96.101.1.3.1.3.1
}

@test "Get wrpcTemperatureTable.1.3.1.1" {
  helper_snmpget_oidnotfound .1.3.6.1.4.1.96.101.1.3.1.3.1.1
}

@test "Get wrpcTemperatureTable.1.3.1.1.1" {
  helper_snmpget_oidnotfound .1.3.6.1.4.1.96.101.1.3.1.3.1.1.1
}

@test "Get next wrpcTemperatureTable" {
  helper_snmpgetnext wrpcTemperatureTable .1.3.6.1.4.1.96.101.1.3.1.2.1
}

@test "Get next wrpcTemperatureTable.0" {
  helper_snmpgetnext wrpcTemperatureTable.0 .1.3.6.1.4.1.96.101.1.3.1.2.1
}

@test "Get next wrpcTemperatureTable.1" {
  helper_snmpgetnext wrpcTemperatureTable.1 .1.3.6.1.4.1.96.101.1.3.1.2.1
}

@test "Get next wrpcTemperatureTable.1.1" {
  helper_snmpgetnext wrpcTemperatureTable.1.1 .1.3.6.1.4.1.96.101.1.3.1.2.1
}


@test "Get next wrpcTemperatureTable.1.1.1" {
  helper_snmpgetnext wrpcTemperatureTable.1.1.1 .1.3.6.1.4.1.96.101.1.3.1.2.1
}


@test "Get next wrpcTemperatureTable.1.1.1.1" {
  helper_snmpgetnext wrpcTemperatureTable.1.1.1.1 .1.3.6.1.4.1.96.101.1.3.1.2.1
}

@test "Get next wrpcTemperatureTable.1.1.1.1.1" {
  helper_snmpgetnext wrpcTemperatureTable.1.1.1.1.1 .1.3.6.1.4.1.96.101.1.3.1.2.1
}


@test "Get next wrpcTemperatureTable.1.1.1.1.2" {
  helper_snmpgetnext wrpcTemperatureTable.1.1.1.1.2 .1.3.6.1.4.1.96.101.1.3.1.2.1
}


@test "Get next wrpcTemperatureTable.1.1.2" {
  helper_snmpgetnext wrpcTemperatureTable.1.1.2 .1.3.6.1.4.1.96.101.1.3.1.2.1
}

@test "Get next wrpcTemperatureTable.1.2" {
  helper_snmpgetnext wrpcTemperatureTable.1.2 .1.3.6.1.4.1.96.101.1.3.1.2.1
}

@test "Get next wrpcTemperatureTable.1.2.0" {
  helper_snmpgetnext wrpcTemperatureTable.1.2.0 .1.3.6.1.4.1.96.101.1.3.1.2.1
}

@test "Get next wrpcTemperatureTable.1.2.1" {
  helper_snmpgetnext wrpcTemperatureTable.1.2.1 .1.3.6.1.4.1.96.101.1.3.1.2.2
}

@test "Get next wrpcTemperatureTable.1.2.1.1" {
  helper_snmpgetnext wrpcTemperatureTable.1.2.1.1 .1.3.6.1.4.1.96.101.1.3.1.2.2
}

@test "Get next wrpcTemperatureTable.1.2.2" {
  helper_snmpgetnext wrpcTemperatureTable.1.2.2 .1.3.6.1.4.1.96.101.1.3.1.2.3
}

@test "Get next wrpcTemperatureTable.1.2.3" {
  helper_snmpgetnext wrpcTemperatureTable.1.2.3 .1.3.6.1.4.1.96.101.1.3.1.2.4
}

@test "Get next wrpcTemperatureTable.1.2.4" {
  helper_snmpgetnext wrpcTemperatureTable.1.2.4 .1.3.6.1.4.1.96.101.1.3.1.3.1
}

@test "Get next wrpcTemperatureTable.1.2.5" {
  helper_snmpgetnext wrpcTemperatureTable.1.2.5 .1.3.6.1.4.1.96.101.1.3.1.3.1
}

@test "Get next wrpcTemperatureTable.1.3" {
  helper_snmpgetnext wrpcTemperatureTable.1.3 .1.3.6.1.4.1.96.101.1.3.1.3.1
}

@test "Get next wrpcTemperatureTable.1.3.0" {
  helper_snmpgetnext wrpcTemperatureTable.1.3.0 .1.3.6.1.4.1.96.101.1.3.1.3.1
}

@test "Get next wrpcTemperatureTable.1.3.0.0" {
  helper_snmpgetnext wrpcTemperatureTable.1.3.0.0 .1.3.6.1.4.1.96.101.1.3.1.3.1
}


@test "Get next wrpcTemperatureTable.1.3.1" {
  helper_snmpgetnext wrpcTemperatureTable.1.3.1 .1.3.6.1.4.1.96.101.1.3.1.3.2
}

@test "Get next wrpcTemperatureTable.1.3.2" {
  helper_snmpgetnext wrpcTemperatureTable.1.3.2 .1.3.6.1.4.1.96.101.1.3.1.3.3
}

@test "Get next wrpcTemperatureTable.1.3.3" {
  helper_snmpgetnext wrpcTemperatureTable.1.3.3 .1.3.6.1.4.1.96.101.1.3.1.3.4
}

@test "Get next wrpcTemperatureTable.1.3.4" {
  helper_snmpgetnext wrpcTemperatureTable.1.3.4 .1.3.6.1.4.1.96.101.1.4.1.0
}

@test "Get next wrpcTemperatureTable.1.3.4.0" {
  helper_snmpgetnext wrpcTemperatureTable.1.3.4.0 .1.3.6.1.4.1.96.101.1.4.1.0
}

@test "Get next wrpcTemperatureTable.1.3.4.1" {
  helper_snmpgetnext wrpcTemperatureTable.1.3.4.1 .1.3.6.1.4.1.96.101.1.4.1.0
}

@test "Get next wrpcTemperatureTable.1.3.4.1.1" {
  helper_snmpgetnext wrpcTemperatureTable.1.3.4.1.1 .1.3.6.1.4.1.96.101.1.4.1.0
}

@test "Get next wrpcTemperatureTable.1.3.5" {
  helper_snmpgetnext wrpcTemperatureTable.1.3.5 .1.3.6.1.4.1.96.101.1.4.1.0
}

@test "Get next wrpcTemperatureTable.1.4" {
  helper_snmpgetnext wrpcTemperatureTable.1.4 .1.3.6.1.4.1.96.101.1.4.1.0
}


@test "Get next wrpcTemperatureTable.1.4.0" {
  helper_snmpgetnext wrpcTemperatureTable.1.4.0 .1.3.6.1.4.1.96.101.1.4.1.0
}


@test "Get next wrpcTemperatureTable.1.4.1" {
  helper_snmpgetnext wrpcTemperatureTable.1.4.1 .1.3.6.1.4.1.96.101.1.4.1.0
}


@test "Get next wrpcTemperatureTable.2" {
  helper_snmpgetnext wrpcTemperatureTable.2 .1.3.6.1.4.1.96.101.1.4.1.0
}


@test "Get next wrpcTemperatureTable.2.0" {
  helper_snmpgetnext wrpcTemperatureTable.2.0 .1.3.6.1.4.1.96.101.1.4.1.0
}

@test "set read only wrpcTemperatureTable.1.2.1" {
  run snmpset $SNMP_OPTIONS $TARGET_IP wrpcTemperatureTable.1.2.1 = 141415
  [ "$status" -eq 2 ]
}
