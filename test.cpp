// (c) 2023 César Godinho
// This code is licensed under MIT license (see LICENSE for details)

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_container_properties.hpp"
#include "stperf.h"
#include <chrono>
#include <cstdint>
#include <pthread.h>
#include <thread>
#include <iostream>

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
    auto tree = cag::PerfTimer::GetCallTree();
    REQUIRE(tree.size() == 0);
    REQUIRE_THAT(cag::PerfTimer::GetCallTreeString(cag::PerfTimer::GetCallTree()), Catch::Matchers::SizeIs(0));
}

TEST_CASE("Simple Scope Guard", "[simple][auto]")
{
    {
        ST_PROF;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto tree = cag::PerfTimer::GetCallTree();
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._hits == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._value > 10);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._value < 11);
}

TEST_CASE("Simple Manual Trigger", "[simple][manual]")
{
    static auto perfc = cag::PerfTimer::MakePerfTimer(__func__, __LINE__, "()");

    perfc->start();
    sleep_10_10();
    perfc->stop();

    auto tree = cag::PerfTimer::GetCallTree();
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._hits == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._value > 100);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._value < 110);
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

    auto tree = cag::PerfTimer::GetCallTree();
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._hits == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.size() == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.at(0)._hits == 3);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.at(0)._value > 30);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.at(0)._value < 33);
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

    auto tree = cag::PerfTimer::GetCallTree();
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._hits == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.size() == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.at(0)._hits == 3);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.at(0)._value > 30);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.at(0)._value < 33);
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

    auto tree = cag::PerfTimer::GetCallTree();
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._hits == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.size() == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.at(0)._hits == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.at(0)._children.at(0)._hits == 1);
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

    auto tree = cag::PerfTimer::GetCallTree();
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._hits == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.size() == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.at(0)._hits == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.at(0)._children.at(0)._hits == 1);
}

TEST_CASE("Reset Mid-counting", "[simple][auto]")
{
    cag::PerfTimer::ResetCounters();
    
    {
        ST_PROF;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cag::PerfTimer::ResetCounters();
    }

    REQUIRE_THAT(cag::PerfTimer::GetCallTreeString(cag::PerfTimer::GetCallTree()), Catch::Matchers::SizeIs(0));
}

TEST_CASE("As Function Argument", "[simple][auto]")
{
    cag::PerfTimer::ResetCounters();

    sleep_arg_return(sleep_return());

    auto tree = cag::PerfTimer::GetCallTree();
    REQUIRE(tree.at(std::this_thread::get_id()).size() == 2);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._hits == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(1)._hits == 1);
    REQUIRE(tree.at(std::this_thread::get_id()).at(0)._children.empty());
    REQUIRE(tree.at(std::this_thread::get_id()).at(1)._children.empty());
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

TEST_CASE("C API", "[capi][auto][simple]")
{
    stperf_ResetCounters();
    uint64_t handle = stperf_StartProf("C Api Test", __LINE__, NULL);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    stperf_StopProf(handle);
    
    stperf_PerfNodeThreadList nodes_all_threads = stperf_GetCallTree();
    const char* report = stperf_GetCallTreeString(nodes_all_threads);

    stperf_PerfNodeList* nodes = stperf_GetThreadRoot(&nodes_all_threads, stperf_GetCurrentThreadId());

    REQUIRE(nodes->_size == 1);
    REQUIRE_THAT(nodes->_elements[0]->_name, Catch::Matchers::Equals("C Api Test"));
    REQUIRE(nodes->_elements[0]->_hits == 1);
    REQUIRE(nodes->_elements[0]->_children._size == 0);
    REQUIRE(nodes->_elements[0]->_children._elements == nullptr);
    REQUIRE(nodes->_elements[0]->_indent == 0);
    REQUIRE(static_cast<cag::PerfNode::Granularity>(nodes->_elements[0]->_granularity) == cag::PerfNode::Granularity::MS);

    stperf_FreeCallTreeString(report);
    stperf_FreeCallTree(nodes_all_threads);
}

TEST_CASE("C API Loop", "[capi][auto][simple]")
{
    stperf_ResetCounters();
    uint64_t handle = stperf_StartProf("C API Loop", __LINE__, NULL);
    
    for(int i = 0; i < 100; i++)
    {
        uint64_t ihandle = stperf_StartProf("Loop", __LINE__, NULL);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        stperf_StopProf(ihandle);
    }

    stperf_StopProf(handle);

    stperf_PerfNodeThreadList nodes_all_threads = stperf_GetCallTree();
    const char* report = stperf_GetCallTreeString(nodes_all_threads);

    std::cout << report << std::endl;

    stperf_FreeCallTreeString(report);
    stperf_FreeCallTree(nodes_all_threads);
}

TEST_CASE("Multi thread", "[auto][mt]")
{
    cag::PerfTimer::ResetCounters();
    std::thread::id tid;
    std::thread* t;
    {
        ST_PROF;
        t = new std::thread([&tid](){
            ST_PROF;
            tid = std::this_thread::get_id();
            for(int i = 0; i < 2; i++) sleep_10_10();
        });
        sleep_10_10(); // Thread 1
    }

    t->join();
    delete t;
    
    auto nodes = cag::PerfTimer::GetCallTree();
    REQUIRE(nodes.size() == 2);
    REQUIRE(nodes.at(std::this_thread::get_id()).at(0)._children.size() == 1);
    REQUIRE(nodes.at(tid).at(0)._children.size() == 1);
}

static void* cthread(void* args)
{
    (void)args;
    uint64_t handle = stperf_StartProf(__func__, __LINE__, NULL);
    
    *(uint64_t*)args = stperf_GetCurrentThreadId();

    sleep_10_10();

    stperf_StopProf(handle);

    pthread_exit(NULL);
}

TEST_CASE("C API Multi thread", "[capi][mt]")
{
    stperf_ResetCounters();
    uint64_t tid;
    pthread_t thread_id;
    uint64_t handle = stperf_StartProf(__func__, __LINE__, NULL);
    
    pthread_create(&thread_id, NULL, cthread, (void*)&tid);
    
    sleep_10_10();
    
    stperf_StopProf(handle);

    pthread_join(thread_id, NULL);

    stperf_PerfNodeThreadList nodes = stperf_GetCallTree();

    stperf_PerfNodeList* main_n   = stperf_GetThreadRoot(&nodes, stperf_GetCurrentThreadId());
    stperf_PerfNodeList* thread_n = stperf_GetThreadRoot(&nodes, tid);

    REQUIRE(nodes._size == 2);
    REQUIRE(main_n->_elements[0]->_children._size == 1);
    REQUIRE(thread_n->_elements[0]->_children._size == 1);

    stperf_FreeCallTree(nodes);
}

// Can also be a lambda function
int func_to_profile(int arg)
{
    ST_PROF;
    // ... function code ...
    return arg;
}

int func_partial_profile(int arg)
{
    ST_PROF;
    {
        ST_PROF_NAMED("partial_scope_1"); // this profiles until scope end
        // ...
    }

    for(int i = 0; i < 10; i++)
    {
        ST_PROF_NAMED("partial_scope_for"); // again until the end of scope
        // ...
    }
    // if using st_prof multiple times inside the same functin it will have the same name
    // using ST_PROF_NAMED is advised for these cases (you can use __func__ for more specific names eg.)
    return arg;
}


TEST_CASE("Readme Print", "[auto][simple]")
{
    // We need to make a scope to be able to see the statistics here
    // Alternatively, you can call start/stop manually
    // Refer to the docs
    {
        ST_PROF;
        (void)func_to_profile(0);
        (void)func_partial_profile(0);
    }

    // Print performance statistics
    std::cout << cag::PerfTimer::GetCallTreeString(cag::PerfTimer::GetCallTree()) << std::endl;
}

