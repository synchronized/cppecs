#pragma once

#include <cppunit/cppunit.h>

#include "cppecs/cppecs.hpp"

struct TestResult;

#define ADD_CHECK(results, a) \
    results.emplace_back(TestResult(a, #a, __FILE__, __LINE__, __FUNCTION__))

#define ADD_CHECKD(results, a, desc) \
    results.emplace_back(TestResult(a, desc, __FILE__, __LINE__, __FUNCTION__))

struct TestResult {
	TestResult(bool result, std::string desc, std::string file, int line, std::string function) 
		: m_result(result), m_desc(desc), m_file(file), m_line(line), m_function(function) {}

	bool m_result;
	std::string m_desc;	
	std::string m_file;
	int m_line;
	std::string m_function;
};

using TestResultList = std::vector<TestResult>;

// Test examples.
class Cppunit_tests: public Cppunit {

	void testEntity();
	void testResource();
	void testSystem();

    void test_list() {
        testEntity();
        testResource();
        testSystem();
    }
};