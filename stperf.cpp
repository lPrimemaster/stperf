// (c) 2023 César Godinho
// This code is licensed under MIT license (see LICENSE for details)

#include "stperf.h"
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stack>
#include <sys/types.h>
#include <thread>
#include <type_traits>
#include <numeric>
#include <type_traits>
#include <vector>

// TODO : (César) Thread parent (init) is not known with this approach

// =================================
// PerfNode
// =================================
decltype(cag::PerfNode::_time_suffix) cag::PerfNode::_time_suffix = {
    { Granularity::S ,  "s" },
    { Granularity::MS, "ms" },
    { Granularity::US, "us" },
    { Granularity::NS, "ns" }
};

void cag::PerfNode::print(std::stringstream& ss) const
{
    // Indent this node from parent count
    for(int i = 0; i < _indent; i++) ss << '\t';
    
    // Print stats
    ss << "-> [" << _name;
    if(_hits > 0) ss << " | x" << _hits;
    ss << "] Execution time : ";
    ss << _value << _time_suffix.at(_granularity) << " (";
    const auto default_precision = std::cout.precision();
    ss << std::setw(3) << std::setprecision(4) << 100 * _pct << std::setprecision(default_precision) << "%).";
    ss << std::endl;
}

// =================================
// PerfTimer
// =================================
decltype(cag::PerfTimer::_scope_stack) cag::PerfTimer::_scope_stack;
decltype(cag::PerfTimer::_parents) cag::PerfTimer::_parents;
decltype(cag::PerfTimer::_scope_stack_guard) cag::PerfTimer::_scope_stack_guard;

cag::PerfTimer::PerfTimer(const std::string& name,
                              int line,
                              const std::string& suffix) : 
    _scope_name(name + suffix), _line(line)
{  }

void cag::PerfTimer::start()
{
    _tp = std::chrono::high_resolution_clock::now();
    addLeaf();
}

void cag::PerfTimer::stop()
{
    // Stack is invalid (Maybe a reset call was issued during this prof)
    if(_scope_stack.empty()) return;
    auto thread_stack_it = _scope_stack.find(std::this_thread::get_id());
    
    // Stack is valid but it was not initialized on this thread
    if(thread_stack_it == _scope_stack.end()) return;
    auto& thread_stack = thread_stack_it->second;

    const std::uint64_t ellapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now() - _tp
    ).count();

    PerfNode* const top_node = thread_stack.top();
    
    // Find time and data
    const auto granularity = FindTimeGranularity(ellapsed);
    top_node->_granularity = granularity;
    top_node->_value = NanosToValue(granularity, ellapsed);
    top_node->_nanos = ellapsed;
    top_node->_name = _scope_name;
    top_node->_indent = static_cast<int>(thread_stack.size()) - 1;
    
    delLeaf();
}

void cag::PerfTimer::addLeaf() const
{
    if(_scope_stack.find(std::this_thread::get_id()) == _scope_stack.end())
    {
        std::lock_guard<std::mutex> lock(_scope_stack_guard);
        _scope_stack.emplace(std::this_thread::get_id(), std::stack<PerfNode*>());
        _parents.emplace(std::this_thread::get_id(), std::vector<PerfNode>());
    }
    auto& thread_stack = _scope_stack.at(std::this_thread::get_id());
    auto& thread_parents = _parents.at(std::this_thread::get_id());

    if(thread_stack.empty())
    {
        thread_parents.emplace_back();
        thread_stack.push(&thread_parents.back());
    }
    else
    {
        PerfNode* const top = thread_stack.top();
        top->_children.emplace_back();
        thread_stack.push(&top->_children.back());
    }
}

void cag::PerfTimer::delLeaf() const
{
    _scope_stack.at(std::this_thread::get_id()).pop();
}

cag::PerfNode::Granularity cag::FindTimeGranularity(std::uint64_t t)
{
    if(t / 0x3B9ACA00ULL > 0) return PerfNode::Granularity::S;
    if(t / 0xF4240ULL > 0)    return PerfNode::Granularity::MS;
    if(t / 0x3E8ULL > 0)      return PerfNode::Granularity::US;
    return PerfNode::Granularity::NS;
}


float cag::NanosToValue(PerfNode::Granularity g, std::uint64_t t)
{
    switch (g)
    {
        case PerfNode::Granularity::NS: return static_cast<float>(t);
        case PerfNode::Granularity::US: return static_cast<float>(t) / 0x3E8ULL;
        case PerfNode::Granularity::MS: return static_cast<float>(t) / 0xF4240ULL;
        case PerfNode::Granularity::S : return static_cast<float>(t) / 0x3B9ACA00ULL;
    }
    return 0.0f;
}

cag::PerfNode::Granularity cag::FindCommonGranularity(PerfNode::Granularity g1, PerfNode::Granularity g2)
{
    using UnderlyingType = std::underlying_type<PerfNode::Granularity>::type;
    PerfNode::Granularity g_common = static_cast<PerfNode::Granularity>(std::max(
        static_cast<UnderlyingType>(g1), static_cast<UnderlyingType>(g2)
    ));
    return g_common;
}

std::shared_ptr<cag::PerfTimer> cag::PerfTimer::MakePerfTimer(const std::string& name, int line, const std::string& suffix)
{
    return std::shared_ptr<PerfTimer>(new PerfTimer(name, line, suffix));
}

static std::uint64_t GetThreadIdSFF(const std::thread::id& id)
{
    static std::unordered_map<std::thread::id, std::uint64_t> sff_id;
    static std::uint64_t inc = 0ULL;
    
    auto sid = sff_id.find(id);
    if(sid != sff_id.end()) return sid->second;

    sff_id.emplace(id, inc);
    return inc++;
}

static void GetStatisticsFullInternal(std::stringstream& ss, const cag::PerfNode& node)
{
    node.print(ss);
    for(const auto& child : node._children)
    {
        GetStatisticsFullInternal(ss, child);
    }
}

void cag::PerfTimer::ResetCounters()
{
    // Invalidate storage and ptr stack for all threads
    std::lock_guard<std::mutex> lock(_scope_stack_guard);
    _parents.clear();
    _scope_stack.clear();
}

static cag::PerfNode ReduceChildren(const std::vector<std::vector<cag::PerfNode>::const_iterator>& nodes)
{
    using PerfNodeConstIt = std::vector<cag::PerfNode>::const_iterator;

    const cag::PerfNode& first = *nodes.front();
    cag::PerfNode reduced;

    reduced._name = first._name;
    reduced._indent = first._indent;
    reduced._pct = first._pct;
    reduced._nanos = std::accumulate(nodes.begin(), nodes.end(), std::uint64_t(0), [](std::uint64_t sum, PerfNodeConstIt node_it) {
        return sum + node_it->_nanos;
    });

    auto granularity = cag::FindTimeGranularity(reduced._nanos);
    reduced._granularity = granularity;
    reduced._value = cag::NanosToValue(granularity, reduced._nanos);
    reduced._hits = nodes.size();

    for(auto& child : nodes)
    {
        reduced._children.insert(reduced._children.end(), child->_children.begin(), child->_children.end());
    }

    return reduced;
}

static std::unordered_map<std::string, std::vector<std::vector<cag::PerfNode>::const_iterator>> ComputeTreeLevelMap(const std::vector<cag::PerfNode>& level)
{
    std::unordered_map<std::string, std::vector<std::vector<cag::PerfNode>::const_iterator>> storage;
    
    // Get child with the same name to collapse
    for(auto it = level.begin(); it != level.end(); it++)
    {
        if(storage.find(it->_name) != storage.end())
        {
            storage[it->_name].push_back(it);
        }
        else
        {
            storage[it->_name] = { it };
        }
    }
    return storage;
}

static cag::PerfNode ComputeNodesCollapse(const cag::PerfNode& root)
{
    // Skip nodes without children
    if(root._children.empty()) return root;

    cag::PerfNode clean_root = root;
    clean_root._children.clear();

    // Get child with the same name to collapse
    std::unordered_map<std::string, std::vector<std::vector<cag::PerfNode>::const_iterator>> storage = ComputeTreeLevelMap(root._children);
    
    // Iterate over each unique children type
    for(auto& kv : storage)
    {
        cag::PerfNode reduced_child = ReduceChildren(kv.second);
        cag::PerfNode reduced_child_collapsed = ComputeNodesCollapse(reduced_child);
        clean_root._children.push_back(reduced_child_collapsed);
    }

    return clean_root;
}

static float CalculateRootRelativePct(const cag::PerfNode& root, const cag::PerfNode& node)
{
    auto granularity = cag::FindCommonGranularity(node._granularity, root._granularity);
    return cag::NanosToValue(granularity, node._nanos) / cag::NanosToValue(granularity, root._nanos);
    // return static_cast<float>(node._nanos) / root._nanos;
}

static void CalculateTreeRelativePct(cag::PerfNode& root, std::vector<cag::PerfNode>& children)
{
    for(auto& child : children)
    {
        child._pct = CalculateRootRelativePct(root, child);
        CalculateTreeRelativePct(root, child._children);
    }
}

std::unordered_map<std::thread::id, std::vector<cag::PerfNode>> cag::PerfTimer::GetCallTree()
{
    using RType = std::unordered_map<std::thread::id, std::vector<cag::PerfNode>>;
    if(_parents.empty()) return RType();

    RType output;
    
    for(auto& thread_parents : _parents)
    {
        if(thread_parents.second.empty()) output.emplace(thread_parents.first, std::vector<cag::PerfNode>());

        std::vector<cag::PerfNode> crunched_data;
        crunched_data.reserve(thread_parents.second.size());
        for(const auto& parent : thread_parents.second)
        {
            // NOTE: (César) Not sure if this is useful here or not
            std::lock_guard<std::mutex> lock(_scope_stack_guard);

            crunched_data.push_back(ComputeNodesCollapse(parent));
            auto& root = crunched_data.back();
            root._pct = CalculateRootRelativePct(root, root);
            CalculateTreeRelativePct(root, root._children);
        }

        // Now crunch the parents (root might not be main/called only once)
        auto storage = ComputeTreeLevelMap(crunched_data);
        
        // Iterate over each unique parent type
        std::vector<cag::PerfNode> thread_tree;
        for(auto& kv : storage)
        {
            cag::PerfNode reduced = ReduceChildren(kv.second);
            thread_tree.push_back(reduced);
        }
        output.emplace(thread_parents.first, thread_tree);
    }
    return output;
}

std::string cag::PerfTimer::GetCallTreeString(const std::unordered_map<std::thread::id, std::vector<PerfNode>>& tree)
{
    std::stringstream ss;
    for(const auto& thread_root : tree)
    {
        ss << "[Thread - " << GetThreadIdSFF(thread_root.first) << "]" << std::endl;
        for(const auto& node : thread_root.second)
        { 
            GetStatisticsFullInternal(ss, node);
        }
    }
    return ss.str();
}


// =================================
// C API
// =================================
static std::unordered_map<std::uint64_t, std::shared_ptr<cag::PerfTimer>> perf_timers;

extern "C" uint64_t stperf_StartProf(const char* name, int line, const char* suffix)
{
    std::uint64_t sid = std::hash<std::string>()(name); // BUG: (César) This could have collisions
    std::string isuffix = suffix != nullptr ? suffix : "";
    auto perf_timer = cag::PerfTimer::MakePerfTimer(std::string(name), line, isuffix);
    perf_timers.emplace(sid, perf_timer);
    perf_timer->start();
    return sid;
}

extern "C" void stperf_StopProf(uint64_t handle)
{
    auto perf_timer = perf_timers.find(handle);
    
    // Exit silently (no error)
    if(perf_timer == perf_timers.end()) return; 
    
    perf_timer->second->stop();
}

static stperf_PerfNode* ToCHeapNode(const cag::PerfNode& node)
{
    stperf_PerfNode* heap_node = new stperf_PerfNode();
    
    using UnderlyingType = std::underlying_type<cag::PerfNode::Granularity>::type;
    heap_node->_granularity = static_cast<UnderlyingType>(node._granularity);
    memcpy(heap_node->_name, node._name.c_str(), std::min(sizeof(heap_node->_name), node._name.size() + 1));
    heap_node->_indent = node._indent;
    heap_node->_nanos  = node._nanos;
    heap_node->_value  = node._value;
    heap_node->_pct    = node._pct;
    heap_node->_hits   = node._hits;

    heap_node->_children._size = node._children.size();
    if(heap_node->_children._size == 0)
    {
        heap_node->_children._elements = nullptr;
        return heap_node;
    }

    heap_node->_children._elements = new stperf_PerfNode*[heap_node->_children._size];
    if(heap_node->_children._elements == nullptr)
    {
        // Silently ignore
        heap_node->_children._size = 0;
        return heap_node;
    }

    for(uint64_t i = 0; i < heap_node->_children._size; i++)
    {
        heap_node->_children._elements[i] = ToCHeapNode(node._children[i]);
    }

    return heap_node;
}

extern "C" stperf_PerfNodeThreadList stperf_GetCallTree()
{
    stperf_PerfNodeThreadList output = { nullptr, 0 };

    auto root_nodes = cag::PerfTimer::GetCallTree();
    
    if(root_nodes.size() == 0) return output;

    output._size = root_nodes.size();
    output._elements = new stperf_PerfNodeList[output._size];

    // Silently ignore
    if(output._elements == nullptr) return output;
    
    size_t n = 0;
    for(auto& root_thread_nodes : root_nodes)
    {
        stperf_PerfNodeList croot = { nullptr, 0, GetThreadIdSFF(root_thread_nodes.first) };
        croot._size = root_thread_nodes.second.size();

        if(croot._size > 0)
        {
            croot._elements = new stperf_PerfNode*[croot._size];

            // Silently ignore
            if(croot._elements != nullptr)
            {
                for(uint64_t i = 0; i < croot._size; i++)
                {
                    croot._elements[i] = ToCHeapNode(root_thread_nodes.second[i]); 
                }
            }
        }
        output._elements[n++] = croot;
    }
    
    return output;
}

extern "C" stperf_PerfNodeList* stperf_GetThreadRoot(const stperf_PerfNodeThreadList* tree, uint64_t tid)
{
    if(tree->_size == 0) return nullptr;
    for(uint64_t i = 0; i < tree->_size; i++)
    {
        if(tree->_elements[i]._thread_id == tid) return &tree->_elements[i];
    }
    return nullptr;
}

static void CPerfNodeSS(stperf_PerfNode node, std::stringstream& ss)
{
    // Indent this node from parent count
    for(int i = 0; i < node._indent; i++) ss << '\t';
    
    // Print stats
    ss << "-> [" << node._name;
    if(node._hits > 0) ss << " | x" << node._hits;
    ss << "] Execution time : ";
    ss << node._value << cag::PerfNode::_time_suffix.at(static_cast<cag::PerfNode::Granularity>(node._granularity)) << " (";
    const auto default_precision = std::cout.precision();
    ss << std::setw(3) << std::setprecision(4) << 100 * node._pct << std::setprecision(default_precision) << "%).";
    ss << std::endl;
}

static void CPerfNodeListSS(stperf_PerfNodeList list, std::stringstream& ss)
{
    for(uint64_t i = 0; i < list._size; i++)
    {
        CPerfNodeSS(*list._elements[i], ss);
        if(list._elements[i]->_children._size != 0) CPerfNodeListSS(list._elements[i]->_children, ss);
    }
}

static std::string GetCallTreeString(stperf_PerfNodeList tree)
{
    std::stringstream ss;
    for(uint64_t i = 0; i < tree._size; i++)
    {
        ss << "[Thread - " << tree._thread_id << "]" << std::endl;
        CPerfNodeSS(*tree._elements[i], ss);
        if(tree._elements[i]->_children._size != 0) CPerfNodeListSS(tree._elements[i]->_children, ss);
    }
    return ss.str();
}

extern "C" const char* stperf_GetCallTreeString(stperf_PerfNodeThreadList tree)
{
    std::stringstream ss;
    std::vector<std::string> output_vec;
    output_vec.reserve(tree._size);
    for(uint64_t i = 0; i < tree._size; i++)
    {
        output_vec.push_back(GetCallTreeString(tree._elements[i]));
    }

    std::string output_s;
    for (const auto& s : output_vec) output_s += s;

    char* output = new char[output_s.size() + 1];
    // Silently ignore
    if(output == nullptr) return nullptr;
    memcpy(output, output_s.c_str(), output_s.size() + 1);
    return output;
}

extern "C" void stperf_FreeCallTreeString(const char* string)
{
    if(string != nullptr) delete[] string;
}

static void FreeCallTreeList(stperf_PerfNodeList root)
{
    if(root._size != 0 && root._elements != nullptr)
    {
        for(uint64_t i = 0; i < root._size; i++)
        {
            FreeCallTreeList(root._elements[i]->_children);
            delete root._elements[i];
        }
        delete[] root._elements;
    }
}

extern "C" void stperf_FreeCallTree(stperf_PerfNodeThreadList tree)
{
    for(uint64_t i = 0; i < tree._size; i++)
    {
        FreeCallTreeList(tree._elements[i]);
    }
}

extern "C" void stperf_ResetCounters()
{
    cag::PerfTimer::ResetCounters();
}


extern "C" uint64_t stperf_GetCurrentThreadId()
{
    return GetThreadIdSFF(std::this_thread::get_id());
}

