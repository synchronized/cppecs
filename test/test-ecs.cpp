#include <iostream>
#include <vector>
#include <string>

#include "cppecs/cppecs.hpp"

#include "test-ecs.hpp"

using namespace cppecs;

struct Name {
	std::string name;
};

struct ID {
	int id;
};

struct Timer{
	int t;
};

struct ResourceTest {
	std::vector<Entity> entities;
	std::vector<bool> exists;
	int curIdx {-1};
};

void setResourceSystem1(Commands& commands, Queryer& queryer) {
	commands.SetResource<ResourceTest>(ResourceTest{});
}

void spawnSystem1(Commands& commands, Queryer& queryer) {
	auto& resTest = queryer.GetResource<ResourceTest>();
	auto& entities = resTest.entities;
	if (entities.empty()) {
		entities.push_back(commands.SpawnAndReturn<Name>(Name{"person1"}));
		entities.push_back(commands.SpawnAndReturn<Name>(Name{"person2"}));
		entities.push_back(commands.SpawnAndReturn<Name, ID>(Name{"person3"}, ID{1}));
		entities.push_back(commands.SpawnAndReturn<ID>(ID{2}));

		resTest.exists.resize(entities.size(), true);
	}
}

void destroySystem1(Commands& commands, Queryer& queryer) {
	auto& resTest = queryer.GetResource<ResourceTest>();
	auto& entities = resTest.entities;
	int idx = ++resTest.curIdx;
	if (idx < resTest.exists.size() && resTest.exists[idx]) {
		commands.Destroy(entities[idx]);
		resTest.exists[idx] = false;
	}
}

void Cppunit_tests::testEntity() {
	World world;
	world.StartUp();

	{
		world.AddSystem(setResourceSystem1);
		world.Update(); 
		world.RemoveSystem(setResourceSystem1);
	}

	{
		world.AddSystem(spawnSystem1);
		world.Update(); //这一次update 执行system
		world.RemoveSystem(spawnSystem1);
	}

	Queryer queryer(world);
	auto& res = queryer.GetResource<ResourceTest>();
	auto& entities = res.entities;
	CHECKS(queryer.Get<Name>(entities[0]).name, "person1");
	CHECKS(queryer.Get<Name>(entities[1]).name, "person2");
	CHECKS(queryer.Get<Name>(entities[2]).name, "person3");
	CHECK(queryer.Get<ID>(entities[2]).id, 1);
	CHECK(queryer.Get<ID>(entities[3]).id, 2);

	CHECK(queryer.Query<ID>().size(), 2);
	CHECK(queryer.Query<Name>().size(), 3);
	CHECK((queryer.Query<Name, ID>().size()), 1);

	{
		world.AddSystem(destroySystem1);
		world.Update();
	}

	CHECK(queryer.Query<ID>().size(), 2);
	CHECK(queryer.Query<Name>().size(), 2);
	CHECK((queryer.Query<Name, ID>().size()), 1);

	{
		world.Update();
	}

	CHECK(queryer.Query<ID>().size(), 2);
	CHECK(queryer.Query<Name>().size(), 1);
	CHECK((queryer.Query<Name, ID>().size()), 1);

	{
		world.Update();
	}
	CHECK(queryer.Query<ID>().size(), 1);
	CHECK(queryer.Query<Name>().size(), 0);
	CHECK((queryer.Query<Name, ID>().size()), 0);

	{
		world.Update();
	}
	CHECK(queryer.Query<ID>().size(), 0);
	CHECK(queryer.Query<Name>().size(), 0);
	CHECK((queryer.Query<Name, ID>().size()), 0);
}

void setResourceSystem2(Commands& commands, Queryer& queryer) {
	if (!queryer.HasResource<Timer>()) {
		commands.SetResource<Timer>(Timer{1});
	}
};

void removeResourceSystem1(Commands& commands, Queryer& queryer) {
	if (queryer.HasResource<Timer>()) {
		commands.RemoveResource<Timer>();
	}
};


void Cppunit_tests::testResource() {
	World world;
	Queryer queryer(world);

	{
		world.AddSystem(setResourceSystem2);
		world.Update();
		world.RemoveSystem(setResourceSystem2);
	}

	CHECKT(queryer.HasResource<Timer>());
	auto& timer = queryer.GetResource<Timer>();
	CHECK(timer.t, 1);

	{
		world.AddSystem(removeResourceSystem1);
		world.Update();
		world.RemoveSystem(removeResourceSystem1);
	}
	CHECKT(!queryer.HasResource<Timer>());
}

struct TestRsult {
	TestRsult(bool result, std::string desc, std::string file, std::string line, std::string function) 
		: m_result(result), m_desc(desc), m_file(file), m_line(line), m_function(function) {}

	bool m_result;
	std::string m_desc;	
	std::string m_file;
	std::string m_line;
	std::string m_function;
};

struct ResTestSystem {
	std::vector<std::string> names = {"person3", "person4", "person5"};
	std::vector<int> ids = {4, 5, 6};
	
	TestResultList tresults;
};

void spawnSystem2(Commands& commands, Queryer& queryer) {
	if (!queryer.HasResource<ResTestSystem>()) {
		auto& res = commands.SetResource<ResTestSystem>(ResTestSystem{
			{"person3", "person4", "person5"},
			{4, 5, 6},
		});

		auto& names = res.names;
		auto& ids = res.ids;

		commands.Spawn<Name>(Name{names[0]});
		commands.Spawn<Name, ID>(Name{names[1]}, ID{ids[0]});
		commands.Spawn<Name, ID>(Name{names[2]}, ID{ids[1]});
		commands.Spawn<ID>(ID{ids[2]});
	}
}

void checkSystem1 (Commands& commands, Queryer& queryer) {
	auto& res = queryer.GetResource<ResTestSystem>();
	auto& tresults = res.tresults;
	auto& names = res.names;
	auto& ids = res.ids;

	std::vector<Entity> entities1 = queryer.Query<Name>();
	ADD_CHECK(tresults, entities1.size() == 3);
	for (Entity entity : entities1) {
		std::string name = queryer.Get<Name>(entity).name;
		auto it = std::find(names.cbegin(), names.cend(), name);
		ADD_CHECKD(tresults, (it != names.cend()), (name+" not found"));
	}

	std::vector<Entity> entities2 = queryer.Query<ID>();
	ADD_CHECK(tresults, entities2.size()==3);
	for (Entity entity : entities2) {
		int id = queryer.Get<ID>(entity).id;
		auto it = std::find(ids.cbegin(), ids.cend(), id);
		ADD_CHECKD(tresults, (it != ids.cend()), (id+" not found"));
	}

	std::vector<Entity> entities3 = queryer.Query<Name, ID>();
	ADD_CHECK(tresults, entities3.size()==2);
	{
		std::vector<std::string> names = {"person4", "person5"};
		std::vector<int> ids = {4, 5};
		for (Entity entity : entities3) {
			int id = queryer.Get<ID>(entity).id;
			std::string name = queryer.Get<Name>(entity).name;
			auto findIdIt = std::find(ids.cbegin(), ids.cend(), id);
			auto findNameIt = std::find(names.cbegin(), names.cend(), name);
			ADD_CHECKD(tresults, (findIdIt != ids.cend()), (id+" not found"));
			ADD_CHECKD(tresults, (findNameIt != names.cend()), (name+" not found"));
		}
	}
}

void Cppunit_tests::testSystem() {
	World world;

	world.StartUp();

	{
		world.AddSystem(spawnSystem2);
		world.Update();
		world.RemoveSystem(spawnSystem2);
	}

	{
		world.AddSystem(checkSystem1);
		world.Update();
		world.RemoveSystem(checkSystem1);
	}

	Queryer queryer(world);
	auto& res = queryer.GetResource<ResTestSystem>();
	auto& tresults = res.tresults;

	for (auto& tresult : tresults) {
		this->check<bool>(tresult.m_result, true, tresult.m_desc, "true", tresult.m_file, tresult.m_line, tresult.m_function);
	}
}

/*
void StartUpSystem(Commands &commands) {
	commands.Spawn<Name>(Name{"person1"});
	commands.Spawn<Name, ID>(Name{"person2"}, ID{1});
	commands.Spawn<ID>(ID{2});
}

void EchoNameSystem(Commands &commands, Queryer &queryer) {
	std::vector<Entity> entities = queryer.Query<Name>();
	std::cout << std::endl << "EchoNameSystem()" << std::endl;
	for (auto entity : entities) {
		std::cout << "  Name: " << queryer.Get<Name>(entity)->name << std::endl;
	}
}

void EchoIDSystem(Commands &commands, Queryer &queryer) {
	std::vector<Entity> entities = queryer.Query<ID>();
	std::cout << std::endl << "EchoIDSystem()" << std::endl;
	for (auto entity : entities) {
		std::cout << "  ID: " << queryer.Get<ID>(entity)->id << std::endl;
	}
}

void EchoNameAndIDSystem(Commands &commands, Queryer &queryer) {
	std::vector<Entity> entities = queryer.Query<Name, ID>();
	std::cout << std::endl << "EchoNameAndIDSystem()" << std::endl;
	for (auto entity : entities) {
		std::cout << "  ID: " << queryer.Get<ID>(entity)->id << ", Name: " << queryer.Get<Name>(entity)->name << std::endl;
	}
}
*/
