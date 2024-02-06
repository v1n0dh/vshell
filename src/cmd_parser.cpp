#include <sstream>

#include "../include/cmd_parser.h"

std::unordered_map<std::string, std::string> oprs = {
	{"PIPE", "|"}, {"AND", "&&"}, {"OR", "||"}};


std::vector<std::string> cmd_parser::split_cmd_into_vector(std::string cmd, char delm) {
	std::vector<std::string> cmd_list;
	std::string temp;
	std::stringstream ss(cmd);

	while (getline(ss, temp, delm)) cmd_list.push_back(temp);

	return cmd_list;
}

void cmd_parser::parse() {
	std::vector<std::string> cmd_tokens = split_cmd_into_vector(this->cmd, ' ');
	std::string command;
	for ( std::string cmd_token : cmd_tokens) {
		if (cmd_token == oprs["PIPE"] ||
			cmd_token == oprs["AND"] ||
			cmd_token == oprs["OR"]) {
			if (command.starts_with(' ')) command.erase(0, 1);
			cmds_queue.push(command);
			opr_queue.push(cmd_token);
			command.clear();
		} else {
			command.append(" ");
			command.append(cmd_token);
		}
	}
	if (command.starts_with(' ')) command.erase(0, 1);
	cmds_queue.push(command);
}
