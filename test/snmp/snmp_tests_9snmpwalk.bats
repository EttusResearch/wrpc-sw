load snmp_test_config

@test "snmpwalk 1.3.6.1.4.1.96.101.1.1" {
  result="$(snmpwalk $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.96.101.1.1 | wc -l)"
  [ "$result" -eq 4 ]
}

@test "snmpwalk 1.3.6.1.4.1.95" {
  result="$(snmpwalk $SNMP_OPTIONS $TARGET_IP 1.3.6.1.4.1.95 | wc -l)"
  [ "$result" -eq 0 ]
}

@test "snmpwalk 1.3.6 count all OIDs" {
  result="$(snmpwalk $SNMP_OPTIONS $TARGET_IP 1.3.6 | wc -l)"
  if [ "$result" -ne $TOTAL_NUM_OIDS ]; then
    echo "Wrong number of OIDs! We expect $TOTAL_NUM_OIDS, while got $result"
    echo "Please check WRPC's configuration. We expect to be in the WRPC:"
    echo $TOTAL_NUM_OIDS_EXPECT_TEXT
    false
  fi
}
