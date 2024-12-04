#pragma once
#include <thread>

// TESTS 
#define TEST_ENTITY_COMPONENT 0
#define TEST_WINDOW 0
#define TEST_RENDERER 1

class test
{
public:
	virtual bool initialize() = 0;
	virtual void run() = 0;
	virtual void shutdown() = 0;
};