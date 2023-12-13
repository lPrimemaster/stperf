#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_container_properties.hpp"
#include "stperf.h"
#include <chrono>
#include <thread>
// #include <iostream>

void sleep_simple()
{
    ST_PROF;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

int sleep_return()
{
    ST_PROF;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return 42;
}

int sleep_arg_return(int arg)
{
    ST_PROF;
    static_cast<void>(arg);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return 42;
}

void sleep_10_10()
{
    ST_PROF;
    for(int n = 0; n < 10; n++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

TEST_CASE("Empty Data", "[simple]")
{
    auto tree = cag::PerfTimer::GetCrunchedData();
    REQUIRE(tree.size() == 0);
    REQUIRE_THAT(cag::PerfTimer::GetStatistics(), Catch::Matchers::SizeIs(0));
}

TEST_CASE("Simple Scope Guard", "[simple][auto]")
{
    {
        ST_PROF;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto tree = cag::PerfTimer::GetCrunchedData();
    REQUIRE(tree.at(0)._hits == 1);
    REQUIRE(tree.at(0)._value > 10);
    REQUIRE(tree.at(0)._value < 11);
}

TEST_CASE("Simple Manual Trigger", "[simple][manual]")
{
    static auto perfc = cag::PerfTimer::MakePerfTimer(__func__, __LINE__, "()");

    perfc->start();
    sleep_10_10();
    perfc->stop();

    auto tree = cag::PerfTimer::GetCrunchedData();
    REQUIRE(tree.at(0)._hits == 1);
    REQUIRE(tree.at(0)._value > 100);
    REQUIRE(tree.at(0)._value < 110);
}

TEST_CASE("Nested Scope Guard", "[nested][auto]")
{
    auto scoped_func = []() { ST_PROF; std::this_thread::sleep_for(std::chrono::milliseconds(10)); };

    {
        ST_PROF;
        scoped_func();
        scoped_func();
        scoped_func();
    }

    auto tree = cag::PerfTimer::GetCrunchedData();
    REQUIRE(tree.at(0)._hits == 1);
    REQUIRE(tree.at(0)._children.size() == 1);
    REQUIRE(tree.at(0)._children.at(0)._hits == 3);
    REQUIRE(tree.at(0)._children.at(0)._value > 30);
    REQUIRE(tree.at(0)._children.at(0)._value < 33);
}

TEST_CASE("Nested Manual Trigger", "[nested][manual]")
{
    cag::PerfTimer::ResetCounters();

    auto scoped_func = []() { 
        static auto perfc = cag::PerfTimer::MakePerfTimer(__func__, __LINE__, "()");
        perfc->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        perfc->stop();
    };

    {
        static auto perfc = cag::PerfTimer::MakePerfTimer(__func__, __LINE__, "()");
        perfc->start();
        scoped_func();
        scoped_func();
        scoped_func();
        perfc->stop();
    }

    auto tree = cag::PerfTimer::GetCrunchedData();
    REQUIRE(tree.at(0)._hits == 1);
    REQUIRE(tree.at(0)._children.size() == 1);
    REQUIRE(tree.at(0)._children.at(0)._hits == 3);
    REQUIRE(tree.at(0)._children.at(0)._value > 30);
    REQUIRE(tree.at(0)._children.at(0)._value < 33);
}

void recurse_auto(int depth)
{
    ST_PROF;
    if(depth > 0) recurse_auto(depth - 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_CASE("Nested Function Call Scope Guard", "[nested][recurse][auto]")
{
    cag::PerfTimer::ResetCounters();
    recurse_auto(2);

    auto tree = cag::PerfTimer::GetCrunchedData();
    REQUIRE(tree.at(0)._hits == 1);
    REQUIRE(tree.at(0)._children.size() == 1);
    REQUIRE(tree.at(0)._children.at(0)._hits == 1);
    REQUIRE(tree.at(0)._children.at(0)._children.at(0)._hits == 1);
}

void recurse_manual(int depth)
{
    static auto perfc = cag::PerfTimer::MakePerfTimer(__func__, __LINE__, "()");
    perfc->start();
    if(depth > 0) recurse_manual(depth - 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    perfc->stop();
}

TEST_CASE("Nested Function Call Manual", "[nested][recurse][manual]")
{
    cag::PerfTimer::ResetCounters();
    recurse_manual(10);

    auto tree = cag::PerfTimer::GetCrunchedData();
    REQUIRE(tree.at(0)._hits == 1);
    REQUIRE(tree.at(0)._children.size() == 1);
    REQUIRE(tree.at(0)._children.at(0)._hits == 1);
    REQUIRE(tree.at(0)._children.at(0)._children.at(0)._hits == 1);
}

TEST_CASE("Reset Mid-counting", "[simple][auto]")
{
    cag::PerfTimer::ResetCounters();
    
    {
        ST_PROF;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cag::PerfTimer::ResetCounters();
    }

    REQUIRE_THAT(cag::PerfTimer::GetStatistics(), Catch::Matchers::SizeIs(0));
}

TEST_CASE("As Function Argument", "[simple][auto]")
{
    cag::PerfTimer::ResetCounters();

    sleep_arg_return(sleep_return());

    auto tree = cag::PerfTimer::GetCrunchedData();
    REQUIRE(tree.size() == 2);
    REQUIRE(tree.at(0)._hits == 1);
    REQUIRE(tree.at(1)._hits == 1);
    REQUIRE(tree.at(0)._children.empty());
    REQUIRE(tree.at(1)._children.empty());
}

// TODO : Missing body
// TEST_CASE("As Function Argument Inside Recurse", "[nested][recurse][auto]")
// {
//     cag::PerfTimer::ResetCounters();
//
//     sleep_arg_return(sleep_return());
//
//     auto tree = cag::PerfTimer::GetCrunchedData();
//     REQUIRE(tree.size() == 2);
//     REQUIRE(tree.at(0)._hits == 1);
//     REQUIRE(tree.at(1)._hits == 1);
//     REQUIRE(tree.at(0)._children.empty());
//     REQUIRE(tree.at(1)._children.empty());
// }

