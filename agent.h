#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include "board.h"
#include "action.h"
#include "weight.h"
#include <math.h>
#include <stdlib.h>
#include<time.h>
int mv = -1;
int original_tile_count = 0;
int bonus_tile_count = 0;
std::vector<int> tile_array;

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

//////////////////////////////////////////////////////////////////////
/**
 * base agent for agents with weight tables
 */
class weight_agent : public agent {
public:
	weight_agent(const std::string& args = "") : agent(args) {
		if (meta.find("init") != meta.end()) // pass init=... to initialize the weight
			init_weights(meta["init"]);
		if (meta.find("load") != meta.end()) // pass load=... to load from a specific file
			load_weights(meta["load"]);
	}
	virtual ~weight_agent() {
		if (meta.find("save") != meta.end()) // pass save=... to save to a specific file
			save_weights(meta["save"]);
	}

protected:
	virtual void init_weights(const std::string& info) {
		for (int i=0; i<4; i++)
			net.emplace_back(pow(15, 6)); // create an empty weight table with size 65536
		//net.emplace_back(65536); // create an empty weight table with size 65536
		// now net.size() == 2; net[0].size() == 65536; net[1].size() == 65536
	}
	virtual void load_weights(const std::string& path) {
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in.is_open()) std::exit(-1);
		uint32_t size;
		in.read(reinterpret_cast<char*>(&size), sizeof(size));
		net.resize(size);
		for (weight& w : net) in >> w;
		in.close();
	}
	virtual void save_weights(const std::string& path) {
		std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!out.is_open()) std::exit(-1);
		uint32_t size = net.size();
		out.write(reinterpret_cast<char*>(&size), sizeof(size));
		for (weight& w : net) out << w;
		out.close();
	}
	friend class player;
protected:
	std::vector<weight> net;
};

/**
 * base agent for agents with a learning rate
 */
class learning_agent : public agent {
public:
	learning_agent(const std::string& args = "") : agent(args), alpha(0.1/32) {
		if (meta.find("alpha") != meta.end())
			alpha = float(meta["alpha"]);
	}
	virtual ~learning_agent() {}

protected:
	float alpha;
};
///////////////////////////////////////////////////



class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

/**
 * random environment
 * add a new random tile to an empty cell
 * 2-tile: 90%
 * 4-tile: 10%
 */

class rndenv : public random_agent {
public:
	rndenv(const std::string& args = "") : random_agent("name=random role=environment " + args),
		space({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }) {}
	virtual action take_action(const board& after) {
		space.clear();
		if (mv == -1)
			space.assign({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 });
		else if (mv == 0)
			space.assign({ 12, 13, 14, 15 });
		else if (mv == 1)
			space.assign({ 0, 4, 8, 12 });
		else if (mv == 2)
			space.assign({ 0, 1, 2, 3});
		else
			space.assign({3, 7, 11, 15});
		
		std::shuffle(space.begin(), space.end(), engine);
	
		int max_tile = 0;
		int last_max_tile;
		for (int i=0; i<16; i++)
			if (after(i) > max_tile){
				last_max_tile = max_tile;
				max_tile = after(i);
			}
		
		for (int pos : space) {
			if (after(pos) != 0) continue;
			if (max_tile >= 7){
				bonus_tile_count++;
				if (1.0*bonus_tile_count / original_tile_count <= 1.0/21){
					
					int bonus_tile_upper_bound = max_tile - 3;  //v_max / 8

					tile = bonus_tile_upper_bound;
					return action::place(pos, tile);
					
				}
				else
					bonus_tile_count--;					
			}
			
			original_tile_count++;
			if ( tile_array.empty() ){
				tile_array = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};
				std::shuffle(tile_array.begin(), tile_array.end(), engine);
				tile = tile_array.back();
				tile_array.pop_back();
			}
			else{
				tile = tile_array.back();
				tile_array.pop_back();
			}

			return action::place(pos, tile);
		}
		return action();
	}

private:
	std::vector<int> space;
	std::vector<int> which_tile_bag =  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
	std::vector<int> which_bonus_tile = {4};
	std::uniform_int_distribution<int> popup;
	board::cell tile;
	
};



class player : public weight_agent {
public:
	std::vector<int> reward_all;
	player(const std::string& args = "") : weight_agent(args) {}
	 
	virtual action take_action(const board& before) {
		board::reward reward = 0;
		board::reward max_reward = reward;
		bool action_flag = false; 
		
		flag = true;
		board tmp;
		int value, max_value;
		for (int op=0; op<=3; op++) {		
			tmp = before;
			
			reward = tmp.slide(op);
			
			if (reward != -1) {
				value = 0;	

				for (int i=0; i<8; i++){
					for (int j=0; j<6; j++){
						s1.push_back(tmp(j));
						s2.push_back(tmp(j+8));
						if (j < 3){
							s3.push_back(tmp(j+5));
							s4.push_back(tmp(j+9));
						}
						else{
							s3.push_back(tmp(j+6));
							s4.push_back(tmp(j+10));	
						}
					}
					value += net[0][pow(15, 5)*s1[0] + pow(15, 4)*s1[1] + pow(15, 3)*s1[2] + pow(15, 2)*s1[3] +15*s1[4] + s1[5]];
					value += net[1][pow(15, 5)*s2[0] + pow(15, 4)*s2[1] + pow(15, 3)*s2[2] + pow(15, 2)*s2[3] +15*s2[4] + s2[5]];
					value += net[2][pow(15, 5)*s3[0] + pow(15, 4)*s3[1] + pow(15, 3)*s3[2] + pow(15, 2)*s3[3] +15*s3[4] + s3[5]];
					value += net[3][pow(15, 5)*s4[0] + pow(15, 4)*s4[1] + pow(15, 3)*s4[2] + pow(15, 2)*s4[3] +15*s4[4] + s4[5]];
					vector<int>().swap(s1);
					vector<int>().swap(s2);
					vector<int>().swap(s3);
					vector<int>().swap(s4);
					tmp.rotate(1);

										
					if (i == 3)
						tmp.reflect_horizontal();
				}

				tmp.reflect_horizontal();
				
				
				reward += value;	
				action_flag = true;
				if (reward >= max_reward){
					max_reward = reward;
					max_value = value;
					action_flag = true;
					mv = op;
				}
			}
		}
		
		if (action_flag){
			reward_all.push_back(max_reward-max_value);
			tmp = before;
			int rr = tmp.slide(mv);
			if (rr == -1)
				for (int op=0; op<4; op++){
					int reward = board(before).slide(op);
					if (reward != -1)
						mv = op;					
				}
			tmp2.push_back(tmp);
				
			return action::slide(mv);
		}
		
		else{
			vector<int> s1_prev, s2_prev, s3_prev, s4_prev;
			vector<int> index;
			for (int j=0; j<reward_all.size(); j++){
				int pre_value = 0, value = 0;
				for (int k=0; k<8; k++){
					for (int l=0; l<6; l++){
						s1.push_back(tmp2[tmp2.size()-1-j](l));
						s2.push_back(tmp2[tmp2.size()-1-j](l+8));
						if (l < 3){
							s3.push_back(tmp2[tmp2.size()-1-j](l+5));
							s4.push_back(tmp2[tmp2.size()-1-j](l+9));
						}
						else{
							s3.push_back(tmp2[tmp2.size()-1-j](l+6));
							s4.push_back(tmp2[tmp2.size()-1-j](l+10));
						}
						
						if (j != 0){
							s1_prev.push_back(tmp2[tmp2.size()-j](l));
							s2_prev.push_back(tmp2[tmp2.size()-j](l+8));
							if (l < 3){
								s3_prev.push_back(tmp2[tmp2.size()-j](l+5));
								s4_prev.push_back(tmp2[tmp2.size()-j](l+9));
							}
							else{
								s3_prev.push_back(tmp2[tmp2.size()-j](l+6));
								s4_prev.push_back(tmp2[tmp2.size()-j](l+10));
							}
						}
					}
					index.push_back(pow(15, 5)*s1[0] + pow(15, 4)*s1[1] + pow(15, 3)*s1[2] + pow(15, 2)*s1[3] +15*s1[4] + s1[5]);
					index.push_back(pow(15, 5)*s2[0] + pow(15, 4)*s2[1] + pow(15, 3)*s2[2] + pow(15, 2)*s2[3] +15*s2[4] + s2[5]);
					index.push_back(pow(15, 5)*s3[0] + pow(15, 4)*s3[1] + pow(15, 3)*s3[2] + pow(15, 2)*s3[3] +15*s3[4] + s3[5]);
					index.push_back(pow(15, 5)*s4[0] + pow(15, 4)*s4[1] + pow(15, 3)*s4[2] + pow(15, 2)*s4[3] +15*s4[4] + s4[5]);
					vector<int>().swap(s1);
					vector<int>().swap(s2);
					vector<int>().swap(s3);
					vector<int>().swap(s4);
					
					if (j != 0){
						int s1_prev_index = pow(15, 5)*s1_prev[0] + pow(15, 4)*s1_prev[1] + pow(15, 3)*s1_prev[2] + pow(15, 2)*s1_prev[3] +15*s1_prev[4] + s1_prev[5];
						int s2_prev_index = pow(15, 5)*s2_prev[0] + pow(15, 4)*s2_prev[1] + pow(15, 3)*s2_prev[2] + pow(15, 2)*s2_prev[3] +15*s2_prev[4] + s2_prev[5];
						int s3_prev_index = pow(15, 5)*s3_prev[0] + pow(15, 4)*s3_prev[1] + pow(15, 3)*s3_prev[2] + pow(15, 2)*s3_prev[3] +15*s3_prev[4] + s3_prev[5];
						int s4_prev_index = pow(15, 5)*s4_prev[0] + pow(15, 4)*s4_prev[1] + pow(15, 3)*s4_prev[2] + pow(15, 2)*s4_prev[3] +15*s4_prev[4] + s4_prev[5];
						for (int m=0; m<4; m++)
							value += net[m][index[m+4*k]];
						pre_value += net[0][s1_prev_index];
						pre_value += net[1][s2_prev_index];
						pre_value += net[2][s3_prev_index];
						pre_value += net[3][s4_prev_index];
						vector<int>().swap(s1_prev);
						vector<int>().swap(s2_prev);
						vector<int>().swap(s3_prev);
						vector<int>().swap(s4_prev);
						tmp2[tmp2.size()-j].rotate(1);
					}
					tmp2[tmp2.size()-1-j].rotate(1);
										
					if (k == 3)
						tmp2[tmp2.size()-1-j].reflect_horizontal();
					if (j != 0 && k == 3)
						tmp2[tmp2.size()-j].reflect_horizontal();
				}
				tmp2[tmp2.size()-1-j].reflect_horizontal();
				if (j != 0)
					tmp2[tmp2.size()-j].reflect_horizontal();
			
				
				if (j != 0)
					for (int i=0; i<4; i++){
						for (int k=0; k<8; k++){
							net[i][index[4*k+i]] += 0.1/32*(pre_value - value + reward_all[reward_all.size()-j]);
						}

					}
				else {
					for (int i=0; i<4; i++){
						for (int k=0; k<8; k++){
							net[i][index[4*k+i]] = 0;
						}

					}
				}
				vector<int>().swap(index);
			}	
			vector<board>().swap(tmp2);
			vector<int>().swap(reward_all);
			return action();
		}
			
	}

private:
	std::array<int, 4> opcode;
	std::vector<int> s1, s2, s3, s4;
	vector<int> rew;
	bool flag = false;
	int count = 0;
	vector<board> tmp2;
};







