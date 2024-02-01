#ifndef _CMD_PARSER_H_
#define _CMD_PARSER_H_

#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <cstdint>

#define INPUT_REDIRECT '<'
#define OUTPUT_REDIRECT '>'

extern std::unordered_map<std::string, std::string> oprs;

class cmd_parser {
private:
	std::string cmd;

public:
	std::queue<std::string> cmds_queue;
	std::queue<std::string> opr_queue;


	cmd_parser(const std::string cmd) : cmd(cmd) {}
	void parse();
	static std::vector<std::string> split_cmd_into_vector(std::string cmd, char delm);
};

#endif
