#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "../include/vshell.h"
#include "../include/cmd_parser.h"

void vshell::start_shell() {
	std::string cmd;
	char* readline_buff;
	int ret;
	int in = STDIN_FILENO;
	int saved_in = dup(STDIN_FILENO), saved_out = dup(STDOUT_FILENO);

	signal(SIGINT, handle_sigint);

	while (true) {
		dup2(saved_in, STDIN_FILENO);
		dup2(saved_out, STDOUT_FILENO);

		readline_buff = readline("vsh> ");
		cmd = readline_buff;
		if (cmd.empty()) continue;

		add_history(readline_buff);

		if (cmd == "exit") break;

		cmd_parser parser(cmd);
		parser.parse();

		std::string cmd_bin = cmd.substr(0, cmd.find(' '));

		// Handling single commands
		if (parser.opr_queue.empty()) {
			if (is_builtin_cmd(cmd_bin)) { execute_builtin_cmd(cmd); continue; }
			if (!check_if_cmd_exists(cmd_bin)) {
				std::cerr << "vsh: command not found: " << cmd_bin << '\n';
				continue;
			}
			execute_cmd(cmd, STDIN_FILENO, STDOUT_FILENO);
			continue;
		}

		// Handling multiple commands chained with operators like "|", "&&", "||"
		std::string prev_opr = "";
		while (!parser.cmds_queue.empty()) {
			cmd = parser.cmds_queue.front();
			std::string opr = "";
			if (!parser.opr_queue.empty()) { opr = parser.opr_queue.front(); parser.opr_queue.pop(); }

			parser.cmds_queue.pop();

			if (is_builtin_cmd(cmd_bin)) { execute_builtin_cmd(cmd); continue; }
			if (!check_if_cmd_exists(cmd_bin)) {
				std::cerr << "vsh: command not found: " << cmd_bin << '\n';
				continue;
			}

			if (opr == oprs["PIPE"]) {
				ret = pipe(pipe_fd);
				if (ret < 0) {
					std::cerr << strerror(errno) << ": Couldn't perform pipe operation\n";
					break;
				}

				execute_cmd(cmd, in, pipe_fd[1]);

				close(pipe_fd[1]);

				in = pipe_fd[0];

				prev_opr = opr;
			} else if (prev_opr == oprs["PIPE"] &&
					   (parser.opr_queue.empty() || parser.opr_queue.front() != oprs["PIPE"])) {

				execute_cmd(cmd, in, STDOUT_FILENO);
			}
		}
	}
}

void vshell::execute_builtin_cmd(std::string cmd) {
	void (vshell::* func) (const std::string& cmd) = builtin_cmd_funcs[cmd.substr(0, cmd.find(' '))];
	(this->*func)(cmd);
	return;
}

int vshell::execute_cmd(std::string cmd, int input, int output) {
	int cmd_status = 0;
	int ret;

	int pid = fork();
	if (pid < 0) {
		std::cerr << std::strerror(errno) << "Couldn't create a new process\n";
		exit(EXIT_FAILURE);
	}

	create_pipes();

	close(in[1]);
	close(out[1]);
	close(err[1]);

	if (pid == 0) {
		int in_fd, out_fd;
		int saved_in_fd, saved_out_fd;
		bool i_redirect = false, o_redirect = false;

		close(in[0]);
		close(out[0]);
		close(err[0]);

		dup2(in[1], STDIN_FILENO);
		dup2(out[1], STDOUT_FILENO);
		dup2(err[1], STDERR_FILENO);

		// pipes are present
		if (pipe_fd[0] != -1 && pipe_fd[1] != -1) {
			if (input != STDIN_FILENO) {
				ret = dup2(input, STDIN_FILENO);
				if (ret < 0) std::cerr << strerror(errno) << ": ret value" << ret << "\n";
				close(input);
			}
			if (output != STDOUT_FILENO) {
				ret = dup2(output, STDOUT_FILENO);
				if (ret < 0) std::cerr << strerror(errno) << ": ret value" << ret << "\n";
				close(output);
			}
			close(pipe_fd[0]);
		}

		if (cmd.find(INPUT_REDIRECT) != std::string::npos) {
			std::vector<std::string> cmd_list = cmd_parser::split_cmd_into_vector(cmd, INPUT_REDIRECT);
			cmd = cmd_list[0];
			if (cmd_list[1].starts_with(' ')) cmd_list[1].erase(0, 1);
			if (cmd_list[1].ends_with(' ')) cmd_list[1].pop_back();

			in_fd = open(cmd_list[1].c_str(), O_RDONLY);
			if (in_fd < 0) {
				std::cerr << std::strerror(errno) << ": Failed to open " << cmd_list[1] << '\n';
			} else {
				saved_in_fd = dup(STDIN_FILENO);
				dup2(in_fd, STDIN_FILENO);
				i_redirect = true;
			}
		}

		if (cmd.find(OUTPUT_REDIRECT) != std::string::npos) {
			std::vector<std::string> cmd_list = cmd_parser::split_cmd_into_vector(cmd, OUTPUT_REDIRECT);
			cmd = cmd_list[0];
			if (cmd_list[1].starts_with(' ')) cmd_list[1].erase(0, 1);
			if (cmd_list[1].ends_with(' ')) cmd_list[1].pop_back();

			out_fd = open(cmd_list[1].c_str(), O_CREAT | O_WRONLY, 0644);
			if (out_fd < 0) {
				std::cerr << std::strerror(errno) << ": Failed to open " << cmd_list[1] << '\n';
			} else {
				saved_out_fd = dup(STDOUT_FILENO);
				dup2(out_fd, STDOUT_FILENO);
				o_redirect = true;
			}
		}

		std::vector<std::string> cmd_list = cmd_parser::split_cmd_into_vector(cmd, ' ');
		if (cmd_list.empty()) return -1;
		const char* cmd_file = cmd_list[0].c_str();
		char *args_list[cmd_list.size() + 1];
		std::memset(args_list, 0, sizeof(args_list));
		std::transform(cmd_list.begin(), cmd_list.end(), args_list, [](const std::string& arg) {
			return const_cast<char*>(arg.c_str());
		});

		cmd_status = execvp(cmd_file, args_list);

		close(in[1]);
		close(out[1]);
		close(err[1]);

		if (i_redirect) { dup2(saved_in_fd, STDIN_FILENO); close(in_fd); }
		if (o_redirect) { dup2(saved_out_fd, STDOUT_FILENO); close(out_fd); }

		exit(0);
	} else {
		waitpid(pid, &cmd_status, 0);

		close(in[0]);
		close(out[0]);
		close(err[0]);
	}

	return cmd_status;
}

void vshell::create_pipes() {
	int ret;

	ret = pipe(in);
	if (ret < 0) {
		std::cerr << std::strerror(errno) << "Failed in creating input pipe\n";
		return;
	}
	ret = pipe(out);
	if (ret < 0) {
		std::cerr << std::strerror(errno) << "Failed in creating output pipe\n";
		return;
	}
	ret = pipe(err);
	if (ret < 0) {
		std::cerr << std::strerror(errno) << "Failed in creating error pipe\n";
		return;
	}
}

bool vshell::is_builtin_cmd(const std::string& cmd) {
	return std::find(builtin_cmds.begin(), builtin_cmds.end(), cmd) != builtin_cmds.end();
}

bool vshell::check_if_cmd_exists(const std::string& cmd) {
	char* path = secure_getenv("PATH");
	std::string path_value(path);
	std::istringstream ss(path_value);
	std::string path_dir;
	struct stat st;
	int ret;

	while (getline(ss, path_dir, ':')) {
		path_dir.append("/" + cmd);
		ret = stat(path_dir.c_str(), &st);
		if (ret == 0) return true;
	}

	return false;
}

void vshell::load_builtin_cmds() {
	// TODO: Need to write better logic
	builtin_cmd_funcs["cd"] = &vshell::_cd;
	builtin_cmd_funcs["export"] = &vshell::_export;
	builtin_cmd_funcs["unset"] = &vshell::_unset;
}

void vshell::_cd(const std::string& cmd) {
	std::string path = cmd.substr(cmd.find(' ') + 1, cmd.length());
	char* pwd = secure_getenv("PWD");
	std::stringstream ss;
	int ret;

	// if user gave cd only, chdir to user's home
	if (cmd == "cd") {
		char* user_home = secure_getenv("HOME");
		ret = chdir(user_home);
		if (ret < 0)
			std::cerr << strerror(errno) << '\n';
		return;
	}

	ret = chdir(path.c_str());
	if (ret < 0) {
		std::cerr << strerror(errno) << '\n';
		return;
	}

	ss << "OLDPWD=" << pwd;
	ret = putenv(const_cast<char*>(ss.str().c_str()));
	if (ret < 0) {
		std::cerr << strerror(errno) << '\n';
		return;
	}

	ss.clear();
	ss << "PWD=" << path;
	ret = putenv(const_cast<char*>(ss.str().c_str()));
	if (ret < 0) {
		std::cerr << strerror(errno) << '\n';
		return;
	}
}

void vshell::_export(const std::string& cmd) {
	std::vector<std::string> cmd_tokens = cmd_parser::split_cmd_into_vector(cmd, ' ');
	// check if the command is correct
	if (cmd_tokens.size() != 2 ||
		cmd_tokens[1].find(' ') == std::string::npos ||
		cmd_tokens[1].find('=') != std::string::npos) {
		std::cerr << "Invalid export syntax\n";
		return;
	}

	std::vector<std::string> variable = cmd_parser::split_cmd_into_vector(cmd_tokens[1], '=');
	shell_variables[variable[0]] = variable[1];

	return;
}

void vshell::_unset(const std::string &cmd) {
	std::vector<std::string> cmd_tokens = cmd_parser::split_cmd_into_vector(cmd, ' ');
	// check if the command is correct
	if (cmd_tokens.size() != 2) {
		std::cerr << "Invalid unset syntax\n";
		return;
	}

	shell_variables.erase(cmd_tokens[1]);

	return;
}
