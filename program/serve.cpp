#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <csignal>
#include <deque>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <print>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <liburing.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "slipstream_signal.h"
#include "slipstream_syscall.h"

#include "address.hpp"
#include "frame.hpp"
#include "key_value.hpp"
#include "listener.hpp"
#include "multishot.hpp"
#include "rest.hpp"
#include "konsens/node.hpp"
#include "konsens/persistent_storage.hpp"
#include "konsens/wire.hpp"
#include "serve.hpp"

namespace konsensd {

namespace {

using node_t = konsens::node<konsens::persistent_storage, key_value>;
using clock = std::chrono::steady_clock;

constexpr unsigned ring_entries = 256;
constexpr unsigned completion_entries_per_ring_entry = 2;
constexpr std::uint64_t max_queued_peer_bytes = std::uint64_t{1} << 26;
constexpr int op_shift = 56;
constexpr std::uint64_t id_mask = (std::uint64_t{1} << op_shift) - 1;

enum class op : std::uint8_t {
    accept_peer = 1,
    accept_client,
    connect,
    receive_peer,
    receive_client,
    send,
    signal,
    sync,
};

struct buffer_ring_shape {
    unsigned short group;
    unsigned count;
    unsigned bytes;
};

constexpr buffer_ring_shape peer_buffer_shape{0, 64, 1U << 16};
constexpr buffer_ring_shape client_buffer_shape{1, 256, 1U << 14};
static_assert(peer_buffer_shape.count + client_buffer_shape.count <= completion_entries_per_ring_entry * ring_entries);
static_assert(std::has_single_bit(peer_buffer_shape.count) && std::has_single_bit(client_buffer_shape.count));

struct buffer_ring {
    io_uring_buf_ring* ring;
    std::vector<char> memory;
    buffer_ring_shape shape;
};

enum class link : std::uint8_t {
    outgoing_peer,
    incoming_peer,
    client,
};

enum class awaited : std::uint8_t {
    nothing,
    registration,
    write,
    read,
};

struct call_in_flight {
    awaited what;
    std::optional<std::uint64_t> session;
};

struct connection {
    int fd;
    link kind;
    konsens::node_id pid;
    bool open;
    bool closing;
    unsigned pending;
    frame_buffer frames;
    std::deque<std::string> out;
    std::size_t out_offset;
    std::uint64_t queued_bytes;
    bool sending;
    std::string http_in;
    std::optional<rest_request> call;
    awaited waits_for;
    std::optional<std::uint64_t> session;
    std::uint64_t request_number;
    bool closes_after_the_send;
};

struct loop {
    const config& c;
    io_uring ring;
    node_t node;
    std::unordered_map<std::uint64_t, std::unique_ptr<connection>> connections;
    std::uint64_t next_id;
    std::unordered_map<konsens::node_id, std::uint64_t> outgoing;
    std::unordered_map<konsens::node_id, std::uint64_t> incoming;
    std::unordered_map<konsens::node_id, socket_address> peer_addresses;
    int peer_listener;
    int client_listener;
    int signal_fd;
    std::vector<konsens::received> batch;
    std::optional<konsens::transaction> step;
    bool stop_requested;
    std::pair<konsens::role, konsens::phase> last_state;
    std::vector<io_uring_cqe> ready;
    std::unordered_map<std::uint64_t, call_in_flight> closed_calls;
    std::unordered_map<std::uint64_t, std::uint64_t> expiries;
    std::uint64_t ticks;
    std::vector<std::uint64_t> closing;
    buffer_ring peer_buffers;
    buffer_ring client_buffers;
    std::vector<std::uint64_t> waiting_for_buffers;
};

std::uint64_t user_data_of(const op o, const std::uint64_t id)
{
    return (static_cast<std::uint64_t>(o) << op_shift) | id;
}

std::string_view name_of(const konsens::role r)
{
    return r == konsens::role::leader ? "leader" : "follower";
}

std::string_view name_of(const konsens::phase p)
{
    switch (p) {
    case konsens::phase::prepare:
        return "prepare";
    case konsens::phase::accept:
        return "accept";
    case konsens::phase::recover:
        return "recover";
    case konsens::phase::none:
        return "none";
    }
    return "unknown";
}

io_uring_sqe* sqe_of(loop& l)
{
    io_uring_sqe* s = io_uring_get_sqe(&l.ring);
    if (s == nullptr) {
        io_uring_submit(&l.ring);
        s = io_uring_get_sqe(&l.ring);
    }
    if (s == nullptr) [[unlikely]] {
        throw std::logic_error("konsensd: the submission queue stays full");
    }
    return s;
}

void arm_accept(loop& l, const int listener, const op o)
{
    io_uring_sqe* s = sqe_of(l);
    io_uring_prep_multishot_accept(s, listener, nullptr, nullptr, SOCK_CLOEXEC);
    io_uring_sqe_set_data64(s, user_data_of(o, 0));
}

void arm_receive(loop& l, const std::uint64_t id, connection& c)
{
    io_uring_sqe* s = sqe_of(l);
    const bool client = c.kind == link::client;
    io_uring_prep_recv_multishot(s, c.fd, nullptr, 0, 0);
    io_uring_sqe_set_flags(s, IOSQE_BUFFER_SELECT);
    io_uring_sqe_set_buf_group(s, client ? client_buffer_shape.group : peer_buffer_shape.group);
    io_uring_sqe_set_data64(s, user_data_of(client ? op::receive_client : op::receive_peer, id));
    c.pending += 1;
}

buffer_ring& buffers_of(loop& l, const op o)
{
    if (o == op::receive_client) {
        return l.client_buffers;
    }
    if (o != op::receive_peer) [[unlikely]] {
        throw std::logic_error("konsensd: a completion that is no receive names a buffer");
    }
    return l.peer_buffers;
}

std::uint16_t buffer_id_of(const buffer_ring& b, const io_uring_cqe& cqe)
{
    const std::uint32_t id = cqe.flags >> IORING_CQE_BUFFER_SHIFT;
    if (id >= b.shape.count) [[unlikely]] {
        throw std::logic_error("konsensd: a completion names a buffer outside its ring");
    }
    return static_cast<std::uint16_t>(id);
}

std::span<char> buffer_of(buffer_ring& b, const std::uint16_t id)
{
    return std::span{b.memory}.subspan(std::size_t{id} * b.shape.bytes, b.shape.bytes);
}

std::span<const char> bytes_in(buffer_ring& b, const io_uring_cqe& cqe)
{
    if (cqe.res <= 0 || static_cast<unsigned>(cqe.res) > b.shape.bytes) [[unlikely]] {
        throw std::logic_error("konsensd: a receive gave more bytes than its buffer holds");
    }
    return buffer_of(b, buffer_id_of(b, cqe)).first(static_cast<std::size_t>(cqe.res));
}

void return_the_buffer(buffer_ring& b, const std::uint16_t id)
{
    io_uring_buf_ring_add(b.ring, buffer_of(b, id).data(), b.shape.bytes, id,
                          io_uring_buf_ring_mask(b.shape.count), 0);
    io_uring_buf_ring_advance(b.ring, 1);
}

void return_the_buffer_of(loop& l, const io_uring_cqe& cqe)
{
    if ((cqe.flags & IORING_CQE_F_BUFFER) == 0) {
        return;
    }
    buffer_ring& b = buffers_of(l, static_cast<op>(cqe.user_data >> op_shift));
    return_the_buffer(b, buffer_id_of(b, cqe));
}

std::expected<buffer_ring, int> buffer_ring_on(io_uring& ring, const buffer_ring_shape shape)
{
    int error = 0;
    io_uring_buf_ring* r = io_uring_setup_buf_ring(&ring, shape.count, shape.group, 0, &error);
    if (r == nullptr) [[unlikely]] {
        return std::unexpected(error);
    }
    buffer_ring b{r, std::vector<char>(std::size_t{shape.count} * shape.bytes), shape};
    for (unsigned id = 0; id < shape.count; ++id) {
        return_the_buffer(b, static_cast<std::uint16_t>(id));
    }
    return b;
}

void arm_the_receives_that_waited_for_buffers(loop& l)
{
    for (const auto id : std::exchange(l.waiting_for_buffers, {})) {
        if (const auto it = l.connections.find(id); it != l.connections.end() && !it->second->closing) {
            arm_receive(l, id, *it->second);
        }
    }
}

void arm_signal(loop& l)
{
    io_uring_sqe* s = sqe_of(l);
    io_uring_prep_poll_add(s, l.signal_fd, POLLIN);
    io_uring_sqe_set_data64(s, user_data_of(op::signal, 0));
}

std::uint64_t added_connection(loop& l, const int fd, const link kind, const konsens::node_id pid)
{
    const std::uint64_t id = l.next_id++;
    auto c = std::make_unique<connection>();
    c->fd = fd;
    c->kind = kind;
    c->pid = pid;
    c->open = kind != link::outgoing_peer;
    l.connections.emplace(id, std::move(c));
    return id;
}

void finish_the_close(loop& l, const std::uint64_t id)
{
    const auto it = l.connections.find(id);
    connection& c = *it->second;
    ::close(c.fd);
    if (c.kind == link::outgoing_peer) {
        if (const auto o = l.outgoing.find(c.pid); o != l.outgoing.end() && o->second == id) {
            l.outgoing.erase(o);
            if (c.open) {
                std::println(std::cerr, "konsensd: the connection to node {} closed", c.pid);
            }
        }
    } else if (c.kind == link::incoming_peer && c.pid != 0) {
        if (const auto i = l.incoming.find(c.pid); i != l.incoming.end() && i->second == id) {
            l.incoming.erase(i);
        }
    } else if (c.kind == link::client) {
        if (c.waits_for != awaited::nothing) {
            l.closed_calls.emplace(id, call_in_flight{c.waits_for, c.session});
        } else if (c.session) {
            l.expiries.emplace(id, *c.session);
        }
    }
    l.connections.erase(it);
}

void begin_the_close(loop& l, const std::uint64_t id, connection& c)
{
    if (!c.closing) {
        c.closing = true;
        ::shutdown(c.fd, SHUT_RDWR);
        l.closing.push_back(id);
    }
}

void finish_the_closes_without_pending_operations(loop& l)
{
    std::erase_if(l.closing, [&l](const std::uint64_t id) {
        if (l.connections.at(id)->pending != 0) {
            return false;
        }
        finish_the_close(l, id);
        return true;
    });
}

void keep_the_step(loop& l, const std::expected<konsens::transaction, std::error_code>& t, const std::string_view call)
{
    if (!t) [[unlikely]] {
        std::println(std::cerr, "konsensd: {} gave the error {}", call, t.error().message());
        return;
    }
    l.step = *t;
}

void queue_on(connection& c, std::string bytes)
{
    c.queued_bytes += bytes.size();
    c.out.push_back(std::move(bytes));
}

void take_the_requests_of(loop& l, std::uint64_t id, connection& c);

void answer(loop& l, const std::uint64_t id, connection& c, std::string response, const bool closes)
{
    queue_on(c, std::move(response));
    c.call.reset();
    c.waits_for = awaited::nothing;
    if (closes) {
        c.closes_after_the_send = true;
        return;
    }
    take_the_requests_of(l, id, c);
}

void answer_with(loop& l, const std::uint64_t id, connection& c, const problem& p)
{
    const bool closes = c.call ? c.call->closes : true;
    answer(l, id, c, problem_response_of(p, closes), closes);
}

void send_the_write(loop& l, const std::uint64_t id, connection& c, const std::uint64_t number)
{
    const auto op = c.call->method == rest_method::put ? key_value_op::put : key_value_op::erase;
    auto t = l.node.client_request(id, *c.session, number, encode_write({op, c.call->key, c.call->body}));
    if (!t) [[unlikely]] {
        answer_with(l, id, c, problem_of(t.error()));
        return;
    }
    l.step = *t;
    c.request_number = number;
    c.waits_for = awaited::write;
}

void start_the_call(loop& l, const std::uint64_t id, connection& c)
{
    if (c.call->method == rest_method::get) {
        auto t = l.node.client_query(id, c.call->key);
        if (!t) [[unlikely]] {
            answer_with(l, id, c, problem_of(t.error()));
            return;
        }
        l.step = *t;
        c.waits_for = awaited::read;
        return;
    }
    if (c.session) {
        send_the_write(l, id, c, c.request_number + 1);
        return;
    }
    auto t = l.node.register_client(id);
    if (!t) [[unlikely]] {
        answer_with(l, id, c, problem_of(t.error()));
        return;
    }
    l.step = *t;
    c.waits_for = awaited::registration;
}

void take_the_requests_of(loop& l, const std::uint64_t id, connection& c)
{
    if (c.call || c.closes_after_the_send || c.closing) {
        return;
    }
    auto r = take_request(c.http_in, l.c.max_body_bytes);
    if (!r) [[unlikely]] {
        c.http_in.clear();
        answer(l, id, c, problem_response_of(r.error(), true), true);
        return;
    }
    if (!*r) {
        return;
    }
    c.http_in.erase(0, (*r)->bytes);
    c.call = std::move(**r);
    start_the_call(l, id, c);
}

void take_a_reply_after_the_close(loop& l, const konsens::client_reply& r)
{
    const auto it = l.closed_calls.find(r.client_id);
    if (it == l.closed_calls.end()) {
        if (l.expiries.erase(r.client_id) == 0 || r.response) {
            return;
        }
        l.expiries.emplace(r.client_id, r.session);
        return;
    }
    const auto [what, session] = it->second;
    l.closed_calls.erase(it);
    if (what == awaited::registration && r.response) {
        l.expiries.emplace(r.client_id, r.session);
    } else if (what == awaited::write && session) {
        l.expiries.emplace(r.client_id, *session);
    }
}

void take_a_reply(loop& l, const konsens::client_reply& r)
{
    const auto it = l.connections.find(r.client_id);
    if (it == l.connections.end() || it->second->kind != link::client) {
        take_a_reply_after_the_close(l, r);
        return;
    }
    connection& c = *it->second;
    if (c.waits_for == awaited::nothing) [[unlikely]] {
        return;
    }
    if (!r.response && c.waits_for == awaited::write && r.response.error() == konsens::client_errc::retry) [[unlikely]] {
        send_the_write(l, r.client_id, c, c.request_number);
        return;
    }
    if (!r.response) [[unlikely]] {
        answer_with(l, r.client_id, c, problem_of(r.response.error()));
        return;
    }
    switch (c.waits_for) {
    case awaited::registration:
        c.session = r.session;
        c.request_number = 0;
        send_the_write(l, r.client_id, c, 1);
        return;
    case awaited::write:
        answer(l, r.client_id, c, response_of(status_no_content, {}, {}, c.call->closes), c.call->closes);
        return;
    case awaited::read: {
        const auto read = decode_read(*r.response);
        if (!read) [[unlikely]] {
            answer_with(l, r.client_id, c, problem_of({}));
            return;
        }
        if (!read->found) {
            answer_with(l, r.client_id, c, {status_not_found, "the key is not there", false});
            return;
        }
        answer(l, r.client_id, c, response_of(status_ok, "application/octet-stream", read->value, c.call->closes),
               c.call->closes);
        return;
    }
    case awaited::nothing:
        return;
    }
}

void answer_the_calls_after_a_failed_commit(loop& l, const std::error_code& e)
{
    for (auto& [id, c] : l.connections) {
        if (c->kind == link::client && c->waits_for != awaited::nothing && !c->closing) {
            c->call->closes = true;
            answer_with(l, id, *c, problem_of(e));
        }
    }
}

void expire_the_closed_sessions(loop& l)
{
    std::erase_if(l.expiries, [&l](const auto& x) {
        auto t = l.node.expire_session(x.first, x.second);
        if (t) {
            l.step = *t;
            return false;
        }
        return t.error() == konsens::client_errc::session_expired;
    });
}

void queue_the_messages(loop& l)
{
    for (const auto& m : l.node.take_outgoing_messages()) {
        const auto to = std::visit([](const auto& x) { return x.to; }, m);
        const auto o = l.outgoing.find(to);
        if (o == l.outgoing.end()) {
            continue;
        }
        connection& c = *l.connections.at(o->second);
        if (!c.open || c.closing || c.queued_bytes > max_queued_peer_bytes) {
            continue;
        }
        const auto bytes = konsens::encode(m);
        if (!bytes || bytes->size() > l.c.max_frame_bytes) [[unlikely]] {
            continue;
        }
        queue_on(c, frame_of(*bytes));
    }
}

void release_the_output(loop& l)
{
    queue_the_messages(l);
    for (const auto& r : l.node.take_client_replies()) {
        take_a_reply(l, r);
    }
}

bool answered_a_write_before_the_sync(loop& l, const konsens::client_reply& r)
{
    const auto it = l.connections.find(r.client_id);
    if (it == l.connections.end() || it->second->kind != link::client || !r.response) {
        return false;
    }
    connection& c = *it->second;
    if (c.waits_for != awaited::write || !c.call) {
        return false;
    }
    const bool closes = c.call->closes;
    queue_on(c, response_of(status_no_content, {}, {}, closes));
    c.call.reset();
    c.waits_for = awaited::nothing;
    c.closes_after_the_send = closes;
    return true;
}

void release_the_output_before_the_sync(loop& l, std::vector<konsens::client_reply>& later,
                                        std::vector<std::uint64_t>& answered)
{
    queue_the_messages(l);
    for (auto& r : l.node.take_client_replies()) {
        if (answered_a_write_before_the_sync(l, r)) {
            answered.push_back(r.client_id);
        } else {
            later.push_back(std::move(r));
        }
    }
}

void log_a_change_of_state(loop& l)
{
    const auto now = l.node.get_sequence_paxos().get_state();
    if (now != l.last_state) {
        std::println(std::cerr, "konsensd: node {} is {} in {}", l.c.pid, name_of(now.first), name_of(now.second));
        l.last_state = now;
    }
}

void arm_the_sends(loop& l);

void commit_the_step(loop& l)
{
    if (!l.batch.empty()) {
        keep_the_step(l, l.node.handle_incoming(std::exchange(l.batch, {})), "handle_incoming");
    }
    if (l.step) {
        const auto t = *std::exchange(l.step, std::nullopt);
        std::vector<konsens::client_reply> later;
        std::vector<std::uint64_t> answered;
        auto r = l.node.commit_phase_one(t);
        if (r) [[likely]] {
            release_the_output_before_the_sync(l, later, answered);
            arm_the_sends(l);
            io_uring_submit(&l.ring);
            r = l.node.commit_phase_two();
        }
        if (!r) [[unlikely]] {
            std::println(std::cerr, "konsensd: the commit gave the error {}", r.error().message());
            answer_the_calls_after_a_failed_commit(l, r.error());
        }
        for (const auto& x : later) {
            take_a_reply(l, x);
        }
        for (const auto id : answered) {
            if (const auto it = l.connections.find(id); it != l.connections.end() && !it->second->closes_after_the_send) {
                take_the_requests_of(l, id, *it->second);
            }
        }
    }
    release_the_output(l);
    log_a_change_of_state(l);
}

void start_a_connect(loop& l, const konsens::node_id pid, const socket_address& to)
{
    const int fd = ::socket(to.storage.ss_family, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) [[unlikely]] {
        return;
    }
    if (uses_tcp(to)) {
        const int one = 1;
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    }
    const std::uint64_t id = added_connection(l, fd, link::outgoing_peer, pid);
    l.outgoing.insert_or_assign(pid, id);
    io_uring_sqe* s = sqe_of(l);
    io_uring_prep_connect(s, fd, reinterpret_cast<const sockaddr*>(&to.storage), to.length);
    io_uring_sqe_set_data64(s, user_data_of(op::connect, id));
    l.connections.at(id)->pending += 1;
}

void connect_the_missing_peers(loop& l)
{
    for (const auto& [pid, address] : l.peer_addresses) {
        if (!l.outgoing.contains(pid)) {
            start_a_connect(l, pid, address);
        }
    }
}

void take_the_frames_of(loop& l, const std::uint64_t id, connection& c)
{
    while (!c.closing) {
        auto f = take_frame(c.frames, l.c.max_frame_bytes);
        if (!f) [[unlikely]] {
            begin_the_close(l, id, c);
            return;
        }
        if (!*f) {
            return;
        }
        if (c.pid == 0) {
            const auto h = decode_hello(**f);
            if (!h || h->cluster_name != l.c.cluster_name || !l.peer_addresses.contains(h->pid)) [[unlikely]] {
                begin_the_close(l, id, c);
                return;
            }
            c.pid = h->pid;
            if (const auto old = l.incoming.find(c.pid); old != l.incoming.end()) {
                begin_the_close(l, old->second, *l.connections.at(old->second));
            }
            l.incoming.insert_or_assign(c.pid, id);
            keep_the_step(l, l.node.reconnected(c.pid), "reconnected");
            continue;
        }
        auto r = konsens::decode(std::move(**f));
        if (!r || r->from != c.pid || r->to != l.c.pid) [[unlikely]] {
            begin_the_close(l, id, c);
            return;
        }
        l.batch.push_back(std::move(*r));
        if (l.batch.size() == konsens::max_batch_size) {
            commit_the_step(l);
        }
    }
}

std::uint64_t most_client_bytes(const config& c)
{
    return std::uint64_t{most_request_head_bytes} + c.max_body_bytes;
}

void handle_a_receive(loop& l, const std::uint64_t id, connection& c, const io_uring_cqe& cqe)
{
    const auto outcome = receive_outcome_of(cqe.res, cqe.flags);
    const bool bytes = outcome == receive_outcome::bytes_and_armed || outcome == receive_outcome::bytes_and_ended;
    if (outcome == receive_outcome::no_buffer && !c.closing) [[unlikely]] {
        l.waiting_for_buffers.push_back(id);
        return;
    }
    if (!bytes || c.closing || c.kind == link::outgoing_peer) {
        begin_the_close(l, id, c);
        return;
    }
    const auto in = bytes_in(buffers_of(l, static_cast<op>(cqe.user_data >> op_shift)), cqe);
    if (c.kind == link::incoming_peer) {
        c.frames.bytes.append(in.begin(), in.end());
        take_the_frames_of(l, id, c);
    } else {
        c.http_in.append(in.begin(), in.end());
        if (c.http_in.size() > most_client_bytes(l.c)) [[unlikely]] {
            begin_the_close(l, id, c);
            return;
        }
        take_the_requests_of(l, id, c);
    }
    if (outcome == receive_outcome::bytes_and_ended && !c.closing) {
        arm_receive(l, id, c);
    }
}

void handle_a_send(loop& l, const std::uint64_t id, connection& c, const int res)
{
    c.sending = false;
    if (res < 0 || c.closing) {
        begin_the_close(l, id, c);
        return;
    }
    c.out_offset += static_cast<std::size_t>(res);
    if (c.out_offset == c.out.front().size()) {
        c.queued_bytes -= c.out.front().size();
        c.out.pop_front();
        c.out_offset = 0;
    }
    if (c.out.empty() && c.closes_after_the_send) {
        begin_the_close(l, id, c);
    }
}

void arm_the_sends(loop& l)
{
    for (auto& [id, c] : l.connections) {
        if (c->sending || c->closing || !c->open || c->out.empty()) {
            continue;
        }
        const std::string& front = c->out.front();
        io_uring_sqe* s = sqe_of(l);
        io_uring_prep_send(s, c->fd, front.data() + c->out_offset, front.size() - c->out_offset, MSG_NOSIGNAL);
        io_uring_sqe_set_data64(s, user_data_of(op::send, id));
        c->pending += 1;
        c->sending = true;
    }
}

void submit_the_sync(loop& l)
{
    const auto r = l.node.take_the_sync_request();
    if (!r) {
        return;
    }
    io_uring_sqe* s = sqe_of(l);
    io_uring_prep_fsync(s, r->file_descriptor, IORING_FSYNC_DATASYNC);
    io_uring_sqe_set_data64(s, user_data_of(op::sync, r->sync_number));
}

void handle_a_completion(loop& l, const io_uring_cqe& cqe)
{
    const auto o = static_cast<op>(cqe.user_data >> op_shift);
    const std::uint64_t id = cqe.user_data & id_mask;
    if (o == op::sync) {
        if (cqe.res < 0) [[unlikely]] {
            l.node.throw_after_a_failed_sync(id, std::error_code{-cqe.res, std::system_category()});
        }
        keep_the_step(l, l.node.the_log_is_synced(id), "the_log_is_synced");
        return;
    }
    if (o == op::signal) {
        int signum = 0;
        while (slipstream_signal_read(l.signal_fd, &signum) == 1) {
            l.stop_requested = true;
        }
        if (!l.stop_requested) {
            arm_signal(l);
        }
        return;
    }
    if (o == op::accept_peer || o == op::accept_client) {
        const bool peer = o == op::accept_peer;
        const auto outcome = accept_outcome_of(cqe.res, cqe.flags);
        if (outcome != accept_outcome::error) {
            const std::uint64_t added = added_connection(l, cqe.res, peer ? link::incoming_peer : link::client, 0);
            arm_receive(l, added, *l.connections.at(added));
        }
        if (outcome != accept_outcome::descriptor_and_armed) {
            arm_accept(l, peer ? l.peer_listener : l.client_listener, o);
        }
        return;
    }
    const auto it = l.connections.find(id);
    if (it == l.connections.end()) [[unlikely]] {
        return;
    }
    connection& c = *it->second;
    if ((cqe.flags & IORING_CQE_F_MORE) == 0) {
        c.pending -= 1;
    }
    switch (o) {
    case op::connect:
        if (cqe.res < 0 || c.closing) {
            begin_the_close(l, id, c);
            return;
        }
        c.open = true;
        std::println(std::cerr, "konsensd: the connection to node {} opened", c.pid);
        queue_on(c, frame_of(encode_hello({l.c.cluster_name, l.c.pid})));
        keep_the_step(l, l.node.reconnected(c.pid), "reconnected");
        arm_receive(l, id, c);
        return;
    case op::receive_peer:
    case op::receive_client:
        handle_a_receive(l, id, c, cqe);
        return;
    case op::send:
        handle_a_send(l, id, c, cqe.res);
        return;
    default:
        return;
    }
}

std::expected<node_t, std::string> started_node(const config& c, const start_options options)
{
    const auto log_directory = c.data_directory / "log";
    const auto own_directory = c.data_directory / "node";
    std::filesystem::create_directories(log_directory);
    std::filesystem::create_directories(own_directory);
    auto store = konsens::persistent_storage::open(log_directory, c.max_store_bytes, ::geteuid());
    if (!store) [[unlikely]] {
        return std::unexpected(std::format("the store does not open: {}", store.error().message()));
    }
    auto own = konsens::node_storage::open(own_directory, ::geteuid());
    if (!own) [[unlikely]] {
        return std::unexpected(std::format("the node storage does not open: {}", own.error().message()));
    }
    auto n = node_t::with(node_config_of(c, options.first_start), std::move(*store), std::move(*own), key_value{},
                          options.nonce);
    if (!n) [[unlikely]] {
        return std::unexpected(std::format("the node does not start: {}", n.error().message()));
    }
    return std::move(*n);
}

int stopped(loop& l)
{
    if (l.node.retired()) {
        std::println(std::cerr, "konsensd: node {} is retired and ends without a clean stop", l.c.pid);
        return exit_stopped;
    }
    if (l.node.get_sequence_paxos().get_error()) [[unlikely]] {
        std::println(std::cerr, "konsensd: node {} has an error of its store and ends without a clean stop", l.c.pid);
        return exit_failed;
    }
    l.node.stop();
    static_cast<void>(l.node.take_client_replies());
    std::println(std::cerr, "konsensd: node {} stopped cleanly", l.c.pid);
    return exit_stopped;
}

}

int serve(const config& c, const start_options options)
{
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGTERM);
    sigaddset(&blocked, SIGINT);
    if (pthread_sigmask(SIG_BLOCK, &blocked, nullptr) != 0) [[unlikely]] {
        std::println(std::cerr, "konsensd: the signals do not block");
        return exit_failed;
    }
    const std::array signums{SIGTERM, SIGINT};
    const int signal_fd = slipstream_signal_open(signums.data(), signums.size());
    if (signal_fd < 0) [[unlikely]] {
        std::println(std::cerr, "konsensd: no descriptor for the signals");
        return exit_failed;
    }
    if (c.sync_by_the_caller && slipstream_syscall_uses_engine() != 0) [[unlikely]] {
        std::println(std::cerr, "konsensd: sync_by_the_caller needs io_uring of the kernel, and this process has none");
        return exit_failed;
    }
    auto n = started_node(c, options);
    if (!n) [[unlikely]] {
        std::println(std::cerr, "konsensd: {}", n.error());
        return exit_failed;
    }
    std::println(std::cerr, "konsensd: node {} started{}", c.pid, n->retired() ? " retired" : "");
    loop l{c, {}, std::move(*n), {}, 1, {}, {}, {}, -1, -1, signal_fd, {}, {}, false, {}, {}, {}, {}, 0, {}, {},
           {}, {}};
    for (const auto& p : c.peers) {
        const auto a = resolve(p.address);
        if (!a) [[unlikely]] {
            std::println(std::cerr, "konsensd: the address {} of node {} does not resolve", p.address, p.pid);
            return exit_failed;
        }
        l.peer_addresses.emplace(p.pid, *a);
    }
    const auto peers = listener_on(c.peer_address);
    if (!peers) [[unlikely]] {
        std::println(std::cerr, "konsensd: no listener on {}: {}", c.peer_address, reason_of(peers.error()));
        return exit_failed;
    }
    const auto clients = listener_on(c.client_address);
    if (!clients) [[unlikely]] {
        std::println(std::cerr, "konsensd: no listener on {}: {}", c.client_address, reason_of(clients.error()));
        return exit_failed;
    }
    l.peer_listener = peers->fd;
    l.client_listener = clients->fd;
    if (const int r = io_uring_queue_init(ring_entries, &l.ring, 0); r < 0) [[unlikely]] {
        std::println(std::cerr, "konsensd: the ring does not start: {}", r);
        return exit_failed;
    }
    auto peer_buffers = buffer_ring_on(l.ring, peer_buffer_shape);
    auto client_buffers = buffer_ring_on(l.ring, client_buffer_shape);
    if (!peer_buffers || !client_buffers) [[unlikely]] {
        std::println(std::cerr, "konsensd: the provided buffer ring does not start: {}",
                     peer_buffers ? client_buffers.error() : peer_buffers.error());
        io_uring_queue_exit(&l.ring);
        return exit_failed;
    }
    l.peer_buffers = std::move(*peer_buffers);
    l.client_buffers = std::move(*client_buffers);
    l.last_state = l.node.get_sequence_paxos().get_state();
    arm_accept(l, l.peer_listener, op::accept_peer);
    arm_accept(l, l.client_listener, op::accept_client);
    arm_signal(l);
    const auto tick = std::chrono::milliseconds{c.tick_milliseconds};
    auto next_tick = clock::now();
    while (!l.stop_requested) {
        const auto wait = l.step ? clock::duration::zero() : std::max(clock::duration::zero(), next_tick - clock::now());
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(wait).count();
        __kernel_timespec ts{ns / 1'000'000'000, ns % 1'000'000'000};
        io_uring_cqe* first = nullptr;
        io_uring_submit_and_wait_timeout(&l.ring, &first, 1, &ts, nullptr);
        unsigned head = 0;
        unsigned seen = 0;
        io_uring_cqe* cqe = nullptr;
        l.ready.clear();
        io_uring_for_each_cqe(&l.ring, head, cqe)
        {
            l.ready.push_back(*cqe);
            seen += 1;
        }
        io_uring_cq_advance(&l.ring, seen);
        for (const auto& e : l.ready) {
            handle_a_completion(l, e);
            return_the_buffer_of(l, e);
            finish_the_closes_without_pending_operations(l);
        }
        arm_the_receives_that_waited_for_buffers(l);
        const auto now = clock::now();
        if (now >= next_tick) {
            connect_the_missing_peers(l);
            keep_the_step(l, l.node.tick(), "tick");
            l.ticks += 1;
            if (l.ticks % l.c.resend_ticks == 0) {
                expire_the_closed_sessions(l);
            }
            next_tick += tick;
            if (next_tick < now) {
                next_tick = now + tick;
            }
        }
        commit_the_step(l);
        submit_the_sync(l);
        arm_the_sends(l);
    }
    const int status = stopped(l);
    io_uring_free_buf_ring(&l.ring, l.peer_buffers.ring, l.peer_buffers.shape.count, l.peer_buffers.shape.group);
    io_uring_free_buf_ring(&l.ring, l.client_buffers.ring, l.client_buffers.shape.count, l.client_buffers.shape.group);
    io_uring_queue_exit(&l.ring);
    for (const auto* file : {&peers->socket_file, &clients->socket_file}) {
        std::error_code e;
        if (!file->empty() && !std::filesystem::remove(*file, e)) [[unlikely]] {
            std::println(std::cerr, "konsensd: the socket file {} stays: {}", file->string(), e.message());
        }
    }
    return status;
}

}
