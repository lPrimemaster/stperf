#include "stperf.h"
#include <iomanip>
#include <iostream>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <numeric>
#include <type_traits>

// TODO : The _scope_stack can be made as of per thread.

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

    const std::uint64_t ellapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now() - _tp
    ).count();

    PerfNode* const top_node = _scope_stack.top();
    
    // Find time and data
    const auto granularity = FindTimeGranularity(ellapsed);
    top_node->_granularity = granularity;
    top_node->_value = NanosToValue(granularity, ellapsed);
    top_node->_nanos = ellapsed;
    top_node->_name = _scope_name;
    top_node->_indent = static_cast<int>(_scope_stack.size()) - 1;
    
    delLeaf();
}

void cag::PerfTimer::addLeaf() const
{
    if(_scope_stack.empty())
    {
        _parents.emplace_back();
        _scope_stack.push(&_parents.back());
    }
    else
    {
        PerfNode* const top = _scope_stack.top();
        top->_children.emplace_back();
        _scope_stack.push(&top->_children.back());
    }
}

void cag::PerfTimer::delLeaf() const
{
    _scope_stack.pop();
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
    // Invalidate storage and ptr stack
    _parents.clear();
    while(!_scope_stack.empty()) _scope_stack.pop();
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

std::vector<cag::PerfNode> cag::PerfTimer::GetCrunchedData()
{
    if(_parents.empty()) return std::vector<cag::PerfNode>();

    std::vector<cag::PerfNode> crunched_data;
    crunched_data.reserve(_parents.size());
    for(const auto& parent : _parents)
    {
        crunched_data.push_back(ComputeNodesCollapse(parent));
        auto& root = crunched_data.back();
        root._pct = CalculateRootRelativePct(root, root);
        CalculateTreeRelativePct(root, root._children);
    }

    // Now crunch the parents (root might not be main/called only once)
    auto storage = ComputeTreeLevelMap(crunched_data);
    
    // Iterate over each unique parent type
    std::vector<cag::PerfNode> output;
    for(auto& kv : storage)
    {
        cag::PerfNode reduced = ReduceChildren(kv.second);
        output.push_back(reduced);
    }

    return output;
}

std::string cag::PerfTimer::GetStatistics(bool debug)
{
    std::stringstream ss;
    std::vector<PerfNode> data;
    
    if(!debug)
    {
        // TODO : GetCrunchedData should set a cached value
        data = GetCrunchedData();
    }
    else
    {
        data = _parents;
    }

    for(const auto& node : data)
    { 
        GetStatisticsFullInternal(ss, node);
    }
    return ss.str();
}
