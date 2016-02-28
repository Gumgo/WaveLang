#ifndef WAVELANG_CONSOLE_COMMANDS_H__
#define WAVELANG_CONSOLE_COMMANDS_H__

#include "common/common.h"

enum e_console_command {
	k_console_command_print,
	k_console_command_set_text_color,
	k_console_command_clear,
	k_console_command_exit,

	k_console_command_count
};

enum e_console_color {
	k_console_color_black,
	k_console_color_dark_blue,
	k_console_color_dark_green,
	k_console_color_dark_aqua,
	k_console_color_dark_red,
	k_console_color_dark_pink,
	k_console_color_dark_yellow,
	k_console_color_dark_white,
	k_console_color_gray,
	k_console_color_blue,
	k_console_color_green,
	k_console_color_aqua,
	k_console_color_red,
	k_console_color_pink,
	k_console_color_yellow,
	k_console_color_white,

	k_console_color_count
};

#endif // WAVELANG_CONSOLE_COMMANDS_H__