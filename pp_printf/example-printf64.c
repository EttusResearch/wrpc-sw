#include <stdint.h>
#include <pp-printf.h>

int main(int argc, char **argv)
{
	long long ll; /* I'd use "int64_t" but gcc wars about formats */

	for (ll = 1000; ll < (1LL << 61); ll *= 1000) {
		pp_printf("positive:     %20lli\n", ll);
		pp_printf("negative:     %20lli\n", -ll);
		pp_printf("neg-unsigned: %20llu\n", -ll);
		pp_printf("pos-hex:        0x%016llx\n", ll);
		pp_printf("pos-HEX:        0x%016llX\n", ll);
		pp_printf("neg-hex:        0x%016Lx\n", -ll);
		pp_printf("\n");
	}
	ll = (1LL <<63) - 1;
	pp_printf("max positive: %20lli\n", ll);
	ll = (1LL << 63);
	pp_printf("max negative: %20lli\n", ll);

	return 0;
}
