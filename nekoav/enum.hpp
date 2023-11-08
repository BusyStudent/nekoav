#pragma once

#include "defs.hpp"
#include <vector>

NEKO_NS_BEGIN

template <typename T>
using Vec = std::vector<T>;

/**
 * @brief State of Element / Pipeline
 * 
 * @details from Stopped <-> Ready <-> Paused <-> Running
 * 
 */
enum class State : int {
    Null    = 0,  //< Default state
    Ready   = 1,  //< Init Compeleted
    Paused  = 2,  //< Paused
    Running = 3,  //< Data is streaming
    Error   = 11, //< Error state
};

enum class StateChange : int {
    NullToReady  = 0,
    ReadyToPaused   = 3,
    PausedToRunning = 5,

    RunningToPaused = 4,
    PausedToReady   = 2,
    ReadyToNull  = 1,

    // Alias
    Initialize      = NullToReady,
    Prepare         = ReadyToPaused,
    Run             = PausedToRunning,
    Pause           = RunningToPaused,
    Stop            = PausedToReady,
    Teardown        = ReadyToNull,

    Invalid         = 10086,
};


inline State GetTargetState(StateChange stateChange) noexcept {
    switch (stateChange) {
        case StateChange::NullToReady:
            return State::Ready;
        case StateChange::ReadyToPaused:
            return State::Paused;
        case StateChange::PausedToRunning:
            return State::Running;
        case StateChange::RunningToPaused:
            return State::Paused;
        case StateChange::PausedToReady:
            return State::Ready;
        case StateChange::ReadyToNull:
            return State::Null;
        default:
            return State::Error;
    }
}
inline State GetPreviousState(StateChange stateChange) noexcept {
    switch (stateChange) {
        case StateChange::NullToReady:
            return State::Null;
        case StateChange::ReadyToPaused:
            return State::Ready;
        case StateChange::PausedToRunning:
            return State::Paused;
        case StateChange::RunningToPaused:
            return State::Running;
        case StateChange::PausedToReady:
            return State::Paused;
        case StateChange::ReadyToNull:
            return State::Ready;
        default:
            return State::Error;
    }
}

inline StateChange GetStateChange(State previousState, State targetState) noexcept {
    if (previousState == State::Null && targetState == State::Ready) {
        return StateChange::NullToReady;
    }
    else if (previousState == State::Ready && targetState == State::Paused) {
        return StateChange::ReadyToPaused;
    }
    else if (previousState == State::Paused && targetState == State::Running) {
        return StateChange::PausedToRunning;
    }
    else if (previousState == State::Running && targetState == State::Paused) {
        return StateChange::RunningToPaused;
    }
    else if (previousState == State::Paused && targetState == State::Ready) {
        return StateChange::PausedToReady;
    }
    else if (previousState == State::Ready && targetState == State::Null) {
        return StateChange::ReadyToNull;
    }
    else {
        return StateChange::Invalid;
    }
}

/**
 * @brief Compute the state change list for [current to target]
 * 
 * @param currentState 
 * @param targetState 
 * @return Vec<StateChange> empty on invalid
 */
inline Vec<StateChange> ComputeStateChanges(State currentState, State targetState) noexcept {
    Vec<StateChange> stateChanges;

    if (currentState == State::Error || targetState == State::Error) {
        return stateChanges;
    }

    // From Stopped to Running
    if (int(targetState) > int(currentState)) {
        // Forward to
        for (auto current = currentState; current < targetState; current = State(int(current) + 1) ){
            stateChanges.push_back(GetStateChange(current, State(int(current) + 1)));
        }
    }
    else {
        for (auto current = currentState; current > targetState; current = State(int(current) - 1) ){
            stateChanges.push_back(GetStateChange(current, State(int(current) - 1)));
        }
    }

    return stateChanges;
}

NEKO_NS_END