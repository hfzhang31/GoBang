#pragma once

struct Square {
	bool empty;
	int color;
    Square(): empty(true), color(-1){}
	void reset();
};
