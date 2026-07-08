/**
 * @file player.hpp
 * @author BusyStudent (fyw90mc@gmail.com)
 * @brief The easy to use player class. high level API.
 * @version 0.1
 * @date 2026-07-05
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#include <nekoav/element.hpp>
#include <nekoav/context.hpp>
#include <memory>

namespace nekoav {

class NEKOAV_API Player {
public:
    Player(Player &&) = default;
    Player();
private:
    struct Impl;

    std::unique_ptr<Impl> d;
};

    
} // namespace nekoav