#include "core/solution_sink.h"
#include <iostream>

namespace dlx {

OstreamSolutionSink::OstreamSolutionSink(std::ostream& stream) : stream_(stream) {}

void OstreamSolutionSink::on_solution(const SolutionView& view)
{
    for (int i = 0; i < view.count; i++)
    {
        const bool last_value = ((i + 1) == view.count);
        stream_ << view.values[i] << (last_value ? '\n' : ' ');
    }
}

void OstreamSolutionSink::flush()
{
    stream_.flush();
}

void CompositeSolutionSink::add_sink(SolutionSink* sink)
{
    if (sink != nullptr)
    {
        sinks_.push_back(sink);
    }
}

bool CompositeSolutionSink::empty() const
{
    return sinks_.empty();
}

void CompositeSolutionSink::on_solution(const SolutionView& view)
{
    for (SolutionSink* sink : sinks_)
    {
        sink->on_solution(view);
    }
}

void CompositeSolutionSink::flush()
{
    for (SolutionSink* sink : sinks_)
    {
        sink->flush();
    }
}

} // namespace dlx

