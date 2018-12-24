#pragma once

#include <random>

namespace KoalaRunBot
{

class Random
{
private:
	std::minstd_rand _rng;

public:
	Random();

	double range(double r);
	int index(int n);
	bool flag(double probability);

	static Random & Instance();
};

}
