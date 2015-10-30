/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "shell.h"

static int cmd_gui(const char *args[])
{
	wrc_ui_mode = UI_GUI_MODE;
	return 0;
}

DEFINE_WRC_COMMAND(gui) = {
	.name = "gui",
	.exec = cmd_gui,
};
