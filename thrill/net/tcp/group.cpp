/*******************************************************************************
 * thrill/net/tcp/group.cpp
 *
 * net::Group is a collection of Connections providing simple MPI-like
 * collectives and point-to-point communication.
 *
 * Part of Project Thrill.
 *
 * Copyright (C) 2015 Timo Bingmann <tb@panthema.net>
 * Copyright (C) 2015 Emanuel Jöbstl <emanuel.joebstl@gmail.com>
 *
 * This file has no license. Only Chuck Norris can compile it.
 ******************************************************************************/

#include <thrill/common/logger.hpp>
#include <thrill/net/tcp/construct.hpp>
#include <thrill/net/tcp/group.hpp>

#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace thrill {
namespace net {
namespace tcp {

std::vector<std::unique_ptr<Group> > Group::ConstructLocalMesh(
    size_t num_hosts) {

    // construct a group of num_hosts
    std::vector<std::unique_ptr<Group> > group(num_hosts);

    for (size_t i = 0; i < num_hosts; ++i) {
        group[i] = std::make_unique<Group>(i, num_hosts);
    }

    // construct a stream socket pair for (i,j) with i < j
    for (size_t i = 0; i != num_hosts; ++i) {
        for (size_t j = i + 1; j < num_hosts; ++j) {
            LOG << "doing Socket::CreatePair() for i=" << i << " j=" << j;

            std::pair<Socket, Socket> sp = Socket::CreatePair();

            sp.first.SetNonBlocking(true);
            sp.second.SetNonBlocking(true);

            group[i]->connections_[j] = Connection(std::move(sp.first));
            group[j]->connections_[i] = Connection(std::move(sp.second));
        }
    }

    return group;
}

std::vector<std::unique_ptr<Group> > Group::ConstructLocalRealTCPMesh(
    size_t num_hosts) {

    // randomize base port number for test
    std::default_random_engine generator(std::random_device { } ());
    std::uniform_int_distribution<int> distribution(10000, 30000);
    const size_t port_base = distribution(generator);

    std::vector<std::string> endpoints;

    for (size_t i = 0; i < num_hosts; ++i) {
        endpoints.push_back("127.0.0.1:" + std::to_string(port_base + i));
    }

    sLOG1 << "Group test uses ports" << port_base << "-" << port_base + num_hosts;

    std::vector<std::thread> threads(num_hosts);

    // we have to create and run threads to construct Group because these create
    // real connections.

    std::vector<std::unique_ptr<Group> > groups(num_hosts);

    for (size_t i = 0; i < num_hosts; i++) {
        threads[i] = std::thread(
            [i, &endpoints, &groups]() {
                        // construct Group i with endpoints
                tcp::Construct(i, endpoints, groups.data() + i, 1);
            });
    }

    for (size_t i = 0; i < num_hosts; i++) {
        threads[i].join();
    }

    return groups;
}

void Group::ExecuteLocalMock(
    size_t num_hosts,
    const std::function<void(Group*)>& thread_function) {

    net::ExecuteLocalMock(
        ConstructLocalMesh(num_hosts),
        thread_function);
}

} // namespace tcp
} // namespace net
} // namespace thrill

/******************************************************************************/
