#include <iostream>

#include "cppecs/cppecs.h"

World::World() {
    std::cout << "hello world";
}

int World::Add(int a, int b){
    return a + b;
}