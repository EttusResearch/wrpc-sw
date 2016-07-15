ifndef TARGET_IP
$(error TARGET_IP is not set)
endif

all: test
prepare: bats/libexec/bats
bats/libexec/bats:
	@echo "bats not installed!"
	@echo "Please perform the following commands:"
	@echo "git clone https://github.com/sstephenson/bats"
# be sure which version we use
	@echo "cd bats; git checkout 03608115"
	@exit 1

test: prepare
	bats/libexec/bats snmp_tests_*.bats

aux_diag: prepare
	bats/libexec/bats snmp_test_aux_diag_*.bats

