load snmp_test_config

@test "Get Counter64 OID (WR-WRPC-MIB::wrpcPtpAsymmetry.0) with SNMPv2" {
  result="$(snmpget $SNMP_OPTIONS -Os $TARGET_IP wrpcPtpAsymmetry.0 | grep "wrpcPtpAsymmetry.0" | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Get Counter64 OID (WR-WRPC-MIB::wrpcPtpAsymmetry.0) with SNMPv1 (not supported)" {
  # SNMP v1 does not support counter64, so it should return oid not found
  result="$(snmpget $SNMP_OPTIONS -Os -v 1 $TARGET_IP wrpcPtpAsymmetry.0 | grep "wrpcPtpAsymmetry.0" | wc -l)"
  [ "$result" -eq 0 ]
}

@test "Getnext OID before Counter64 WR-WRPC-MIB::wrpcPtpAsymmetry.0 with SNMPv1" {
  # should point after the Counter64 (wrpcPtpRTTErrCnt.0) to the wrpcPtpTx.0
  result="$(snmpgetnext $SNMP_OPTIONS -Os -v 1 $TARGET_IP wrpcPtpRTTErrCnt.0 | grep "wrpcPtpTx.0" | wc -l)"
  [ "$result" -eq 1 ]
}

@test "Getnext Counter64 OID WR-WRPC-MIB::wrpcPtpAsymmetry.0 with SNMPv1" {
  # should point after Counter64 (wrpcPtpAsymmetry.0) to the wrpcPtpTx.0
  result="$(snmpgetnext $SNMP_OPTIONS -Os -v 1 $TARGET_IP wrpcPtpAsymmetry.0 | grep "wrpcPtpTx.0" | wc -l)"
  [ "$result" -eq 1 ]
}
