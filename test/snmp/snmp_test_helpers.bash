helper_snmpget() {
  result="$(snmpget $SNMP_OPTIONS $TARGET_IP $1 | grep "$2" | wc -l)"
  [ "$result" -eq 1 ]
}

helper_snmpget_oidnotfound() {
  run snmpget $SNMP_OPTIONS $TARGET_IP $1
  [ "$status" -eq 2 ]
}

helper_snmpget_oidfound() {
  result="$(snmpget $SNMP_OPTIONS $TARGET_IP $1 | grep "$1" | wc -l)"
  [ "$result" -eq 1 ]
}

helper_snmpgetnext() {
  result="$(snmpgetnext $SNMP_OPTIONS $TARGET_IP $1 | grep "$2" | wc -l)"
  [ "$result" -eq 1 ]
}


helper_snmpset() {
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP $1 = "$2" | grep "$2" | wc -l)"
  [ "$result" -eq 1 ]
}

helper_erase_sfp_database() {
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = eraseFlash | grep applySuccessful | wc -l)"
  [ "$result" -eq 1 ]
}
