// (c) 2023 César Godinho
// This code is licensed under MIT license (see LICENSE for details)

#include <chrono>
#include <cstdint>
#include <string>
#include <stack>
#include <vector>
#include <map>
#include <memory>
#include <sstream>

namespace cag
{
    struct PerfNode
    {
        enum class Granularity { S, MS, US, NS } _granularity;
        static const std::map<Granularity, std::string> _time_suffix;
        float _value;
        float _pct;
        std::uint64_t _nanos;
        std::string _name;
        int _indent;
        std::vector<PerfNode> _children;
        std::uint64_t _hits;

        void print(std::stringstream& ss) const;
    };

    class PerfTimer
    {
    private:
        PerfTimer(const std::string& name, int line, const std::string& suffix);
        PerfTimer(const PerfTimer& st) = delete;
        PerfTimer(PerfTimer&& t) = delete;

        std::chrono::high_resolution_clock::time_point _tp;
        std::string _scope_name;
        int _line;
        static std::stack<PerfNode*> _scope_stack;
        static std::vector<PerfNode> _parents;
    
        void addLeaf() const;
        void delLeaf() const;

    public:
        static std::shared_ptr<PerfTimer> MakePerfTimer(const std::string& name, int line, const std::string& suffix = "");
        static void ResetCounters();
        static std::string GetCallTreeString(const std::vector<PerfNode>& tree);
        static std::vector<PerfNode> GetCallTree();
        void start();
        void stop();
    };

    template<typename T>
    class ScopeGuard
    {
    public:
        explicit ScopeGuard(T& t) : t(t) { t->start(); }
        ~ScopeGuard() { t->stop(); }
    private:
        T& t;
    };

    PerfNode::Granularity FindTimeGranularity(std::uint64_t t);
    PerfNode::Granularity FindCommonGranularity(PerfNode::Granularity g1, PerfNode::Granularity g2);
    float NanosToValue(PerfNode::Granularity g, std::uint64_t t);
}

extern "C" struct stperf_PerfNode;

extern "C" struct stperf_PerfNodeList
{
    stperf_PerfNode** _elements;
    uint64_t          _size;
};

extern "C" struct stperf_PerfNode
{
    int      _granularity;
    float    _value;
    float    _pct;
    uint64_t _nanos;
    char     _name[128]; // NOTE : Names will get cropped in C if more than 128 chars long
    int      _indent;
    uint64_t _hits;
    stperf_PerfNodeList _children;
};

extern "C" uint64_t            stperf_StartProf(const char* name, int line, const char* suffix);
extern "C" void                stperf_StopProf(uint64_t handle);
extern "C" stperf_PerfNodeList stperf_GetCallTree();
extern "C" const char*         stperf_GetCallTreeString(stperf_PerfNodeList tree);
extern "C" void                stperf_FreeCallTreeString(const char* string);
extern "C" void                stperf_FreeCallTree(stperf_PerfNodeList root);
extern "C" void                stperf_ResetCounters();

#define _CAT_NAME(x,y) x##y
#define CAT_NAME(x,y) _CAT_NAME(x,y)
#define ST_PROF static auto CAT_NAME(_perfcounter_,__LINE__) = cag::PerfTimer::MakePerfTimer(__func__, __LINE__, "()"); \
                cag::ScopeGuard<std::shared_ptr<cag::PerfTimer>> CAT_NAME(_scopeguard_,__LINE__)(CAT_NAME(_perfcounter_,__LINE__))

#define ST_PROF_NAMED(x) static auto CAT_NAME(_perfcounter_,__LINE__) = cag::PerfTimer::MakePerfTimer(x, __LINE__); \
                         cag::ScopeGuard<std::shared_ptr<cag::PerfTimer>> CAT_NAME(_scopeguard_,__LINE__)(CAT_NAME(_perfcounter_,__LINE__))
