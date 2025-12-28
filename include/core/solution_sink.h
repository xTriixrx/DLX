#ifndef DLX_SOLUTION_SINK_H
#define DLX_SOLUTION_SINK_H

#include <ostream>
#include <vector>

namespace dlx::sink {

struct SolutionView
{
    char* const* values;
    int count;
};

class SolutionSink
{
public:
    virtual ~SolutionSink() = default;
    virtual void on_solution(const SolutionView& view) = 0;
    virtual void flush() {}
};

class OstreamSolutionSink final : public SolutionSink
{
public:
    explicit OstreamSolutionSink(std::ostream& stream);
    void on_solution(const SolutionView& view) override;
    void flush() override;

private:
    std::ostream& stream_;
};

class CompositeSolutionSink final : public SolutionSink
{
public:
    void add_sink(SolutionSink* sink);
    bool empty() const;
    void on_solution(const SolutionView& view) override;
    void flush() override;

private:
    std::vector<SolutionSink*> sinks_;
};

} // namespace dlx

#endif
