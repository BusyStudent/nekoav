#include "tracer.hpp"

NEKO_NS_BEGIN

PrintElementTracer::PrintElementTracer(FILE *stream) : mStream(stream) {}

void PrintElementTracer::received(ActionData actionData) {
    switch (actionData.event_type)
    {
    case ElementEventType::UnknownEvent:
        _output("[%u][%s:%u (%s)][%s][%s]: unknow stage event happen, message: %s\n", 
                actionData.timestamp, 
                actionData.location.file_name(), 
                actionData.location.line(), 
                actionData.location.function_name(), 
                (actionData.thread == nullptr ? "mainThread" : actionData.thread->name().data()), 
                actionData.element->name().c_str(),
                actionData.event_msg.c_str());
        break;
    case ElementEventType::StageBegin:
        _output("[%u][%s:%u (%s)][%s][%s]: %s stage begin\n", 
                actionData.timestamp, 
                actionData.location.file_name(), 
                actionData.location.line(), 
                actionData.location.function_name(), 
                (actionData.thread == nullptr ? "mainThread" : actionData.thread->name().data()), 
                actionData.element->name().c_str(),
                actionData.event_msg.c_str());
        break;
    case ElementEventType::StageEnd:
        _output("[%u][%s:%u (%s)][%s][%s]: %s stage end\n", 
                actionData.timestamp, 
                actionData.location.file_name(), 
                actionData.location.line(), 
                actionData.location.function_name(), 
                (actionData.thread == nullptr ? "mainThread" : actionData.thread->name().data()), 
                actionData.element->name().c_str(),
                actionData.event_msg.c_str());
        break;
    default:
        _output("[%u][%s:%u (%s)][%s][%s]: %s\n", 
                actionData.timestamp, 
                actionData.location.file_name(), 
                actionData.location.line(), 
                actionData.location.function_name(), 
                (actionData.thread == nullptr ? "mainThread" : actionData.thread->name().data()), 
                actionData.element->name().c_str(),
                actionData.event_msg.c_str());
        break;
    }
}

void PrintElementTracer::_output(const char *format, ...) {
    va_list varg;
    va_start(varg, format);
    ::fprintf(mStream, "TRACE >>> ");
    ::vfprintf(mStream, format, varg);
    va_end(varg);

    ::fflush(mStream);
}

ElementTCTrack::ElementTCTrack(FILE *stream) : mStream(stream) {
    ::fprintf(mStream, "sequenceDiagram\n");
}

void ElementTCTrack::received(ActionData actionData) {
    if (!elements.contains(actionData.element)) {
        elements.insert(std::make_pair(actionData.element, std::to_string((uintptr_t)actionData.element)));
        if (!actionData.element->name().empty()) {
            _output("participant %s as %s\n", elements[actionData.element].c_str(), actionData.element->name().c_str());    
        } else {
            _output("participant %s\n", elements[actionData.element].c_str());
        }
    }
    switch (actionData.event_type)
    {
    case ElementEventType::UnknownEvent:
        break;
    case ElementEventType::StageBegin:
        _output("activate %s\n", elements[actionData.element].c_str());
        break;
    case ElementEventType::StageEnd:
        _output("%s->>%s: %s\n", elements[actionData.element].c_str(), elements[actionData.element].c_str(), actionData.event_msg.c_str());
        _output("deactivate %s\n", elements[actionData.element].c_str());
        break;
    default:
        break;
    }
}

void ElementTCTrack::_output(const char *format, ...) {
    va_list varg;
    va_start(varg, format);
    ::vfprintf(mStream, format, varg);
    va_end(varg);

    ::fflush(mStream);
}

NEKO_NS_END