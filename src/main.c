#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

int main(void)
{
	const struct device *epd = DEVICE_DT_GET(DT_NODELABEL(e_paper));

	if (!device_is_ready(epd)) {
		return 0;
	}

	display_blanking_off(epd);
	display_clear(epd);
	return 0;
}
