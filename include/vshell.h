#ifndef _VSH_H_
#define _VSH_H_

#include <iostream>
#include <vector>
#include <unordered_map>

#include "../include/cmd_parser.h"

class vshell {
private:
	int in[2], out[2], err[2];
	int pipe_fd[2];
	// This can be further added
	std::vector<std::string> builtin_cmds = {"cd", "exec", "echo", "eval", "export", "unset"};
	std::unordered_map<std::string, void (vshell::*) (const std::string&)> builtin_cmd_funcs;
	std::unordered_map<std::string, std::string> shell_variables;

	void create_pipes();
	std::vector<std::string> split_cmd_into_vector(std::string cmd, char delm);

public:
	void load_builtin_cmds();
	void execute_cmd(std::string cmd, int input, int output);
	void execute_builtin_cmd(std::string cmd);
	void start_shell();
	bool check_if_cmd_exists(const std::string& cmd);
	bool is_builtin_cmd(const std::string& cmd);

	// builtin command funcs
	void _cd(const std::string& cmd);
	void _export(const std::string& cmd);
	void _unset(const std::string& cmd);
};

#endif
