#include "test-ecs.h"

#include "cppecs/cppecs.h"

// Test examples.
void Cppunit_tests::single_test() {
	/*
	// Integral type match check.
	CHECK(2 + 2, 4);

	// Boolean type value check.
	CHECKT(2 + 2 == 4);

	// String match check.
	CHECKS("a" "b", "ab");

	// Stdin override example.
	test_cin("2\n2");
	CHECK((new test_class)->calculate(), 4);
	*/

	auto world = new World();
	CHECK(world->Add(1, 2), 3);
}