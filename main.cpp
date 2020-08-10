#include "mvThreadPool.h"
#include <iostream>
#include <chrono>

void SomeFunction()
{
	std::cout << "Some Function" << std::endl;
}

void SomeVarFunction(int x, int b)
{
	std::cout << "Some Var Function with args: " << x << " and " << b << std::endl;
}

class Entity
{

	int m_age = 50;

public:

	int getAge(int seconds)
	{
		std::this_thread::sleep_for(std::chrono::seconds(seconds));
		return m_age;
	}

};

int main()
{
	// create threadpool
	auto threadpool = Marvel::mvThreadPool();

	// Submit a function
	//-----------------------------------------------------------------------------
	threadpool.submit(&SomeFunction);

	// Submit a function with arguments
	//-----------------------------------------------------------------------------
	threadpool.submit(std::bind(&SomeVarFunction, 5, 2));

	// Create an Entity and submit a method call and wait
	//-----------------------------------------------------------------------------
	auto entity = Entity();
	auto fut = threadpool.submit(std::bind(&Entity::getAge, entity, 3));
	int age = fut.get(); // this will wait until the call returns

	// print returned value
	std::cout << "Future ready and returned: " << age << std::endl;
}