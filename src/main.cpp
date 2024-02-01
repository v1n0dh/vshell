#include <iostream>

#include "../include/vshell.h"

int main() {
	vshell vsh;

	vsh.load_builtin_cmds();
	vsh.start_shell();

	return 0;
}
