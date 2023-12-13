#include <chrono>
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
        static std::string GetStatistics(bool debug = false);
        static std::vector<PerfNode> GetCrunchedData();
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

#define _CAT_NAME(x,y) x##y
#define CAT_NAME(x,y) _CAT_NAME(x,y)
#define ST_PROF static auto CAT_NAME(_perfcounter_,__LINE__) = cag::PerfTimer::MakePerfTimer(__func__, __LINE__, "()"); \
                cag::ScopeGuard<std::shared_ptr<cag::PerfTimer>> CAT_NAME(_scopeguard_,__LINE__)(CAT_NAME(_perfcounter_,__LINE__))

#define ST_PROF_NAMED(x) static auto CAT_NAME(_perfcounter_,__LINE__) = cag::PerfTimer::MakePerfTimer(x, __LINE__); \
                         cag::ScopeGuard<std::shared_ptr<cag::PerfTimer>> CAT_NAME(_scopeguard_,__LINE__)(CAT_NAME(_perfcounter_,__LINE__))
