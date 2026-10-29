#include <arpa/inet.h>
#include <endian.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <rclcpp/rclcpp.hpp>

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

static std::atomic<bool> g_running{true};

void on_sigint(int) {
    g_running = false;
}

class UDPReceiver : public rclcpp::Node {
public:
    explicit UDPReceiver(int port)
        : Node("udp_receiver_node"), port_(port) {
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);

        if (sock_ < 0) {
            std::perror("socket");
            throw std::runtime_error("Failed to create UDP socket.");
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(static_cast<uint16_t>(port_));

        if (bind(sock_, reinterpret_cast<sockaddr*>(&addr),
                 sizeof(addr)) < 0) {
            std::perror("bind");
            close(sock_);
            sock_ = -1;
            throw std::runtime_error(
                "Failed to bind UDP socket to port " +
                std::to_string(port_));
        }

        RCLCPP_INFO(
            get_logger(), "UDPReceiver listening on port %d...", port_);
    }

    ~UDPReceiver() override {
        if (sock_ >= 0) {
            close(sock_);
        }
        RCLCPP_INFO(get_logger(), "UDPReceiver socket closed.");
    }

    void start_listening() {
        uint8_t buf[kPacketSize];
        sockaddr_in client_addr{};

        while (g_running && rclcpp::ok()) {
            socklen_t client_len = sizeof(client_addr);

            ssize_t len = recvfrom(
                sock_, buf, sizeof(buf), 0,
                reinterpret_cast<sockaddr*>(&client_addr),
                &client_len);

            if (len < 0) {
                if (!g_running || !rclcpp::ok()) {
                    break;
                }

                std::perror("recvfrom");
                continue;
            }

            if (static_cast<size_t>(len) != kPacketSize) {
                RCLCPP_WARN(
                    get_logger(),
                    "Unexpected packet size: %zd", len);
                continue;
            }

            if (std::memcmp(buf, kHeader, sizeof(kHeader)) != 0) {
                RCLCPP_WARN(
                    get_logger(), "Invalid packet header.");
                continue;
            }

            uint32_t data_le[4];
            std::memcpy(data_le, buf + 4, sizeof(data_le));

            int32_t left_ticks =
                static_cast<int32_t>(le32toh(data_le[0]));
            int32_t right_ticks =
                static_cast<int32_t>(le32toh(data_le[1]));
            int32_t vx =
                static_cast<int32_t>(le32toh(data_le[2]));
            int32_t vy =
                static_cast<int32_t>(le32toh(data_le[3]));

            RCLCPP_INFO(
                get_logger(),
                "Left ticks: %d | Right ticks: %d | "
                "Vx: %d cm/s | Vy: %d cm/s",
                left_ticks, right_ticks, vx, vy);
        }
    }

private:
    static constexpr char kHeader[4] = {'I', 'R', 'I', 'S'};
    static constexpr size_t kPacketSize = 20;

    int sock_ = -1;
    int port_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    std::signal(SIGINT, on_sigint);

    int port = (argc > 1) ? std::atoi(argv[1]) : 12152;

    try {
        auto receiver = std::make_shared<UDPReceiver>(port);
        receiver->start_listening();
    } catch (const std::exception& e) {
        RCLCPP_ERROR(
            rclcpp::get_logger("main"), "Error: %s", e.what());
    }

    if (rclcpp::ok()) {
        rclcpp::shutdown();
    }

    return 0;
}
