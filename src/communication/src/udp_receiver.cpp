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
#include <cstring>
#include <stdexcept>
#include <string>

class UDPReceiver : public rclcpp::Node {
public:
    UDPReceiver(int port) : Node("udp_receiver_node"), port_(port) {
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0) {
            std::perror("socket");
            throw std::runtime_error("Failed to create UDP socket.");
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(static_cast<uint16_t>(port_));

        if (bind(sock_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
            close(sock_);
            std::perror("bind");
            throw std::runtime_error("Failed to bind UDP socket to port " + std::to_string(port_));
        }

        RCLCPP_INFO(this->get_logger(), "UDPReceiver listening on port %d...", port_);
    }

    ~UDPReceiver() {
        if (sock_ >= 0) {
            close(sock_);
        }
        RCLCPP_INFO(this->get_logger(), "UDPReceiver socket closed.");
    }

    void start_listening(std::atomic<bool>& running_flag) {
        uint8_t buf[kPacketSize];
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        while (running_flag && rclcpp::ok()) {
            ssize_t len = recvfrom(sock_, buf, sizeof(buf), 0,
                                 reinterpret_cast<sockaddr *>(&client_addr), &client_len);
            if (len < 0) {
                if (!running_flag) break;
                std::perror("recvfrom");
                continue;
            }

            if (static_cast<size_t>(len) == kPacketSize) {
                // Validasi Header 'IRIS'
                if (std::memcmp(buf, kHeader, sizeof(kHeader)) == 0) {
                    uint32_t pwm_le, steer_le;
                    std::memcpy(&pwm_le, buf + 4, sizeof(pwm_le));
                    std::memcpy(&steer_le, buf + 8, sizeof(steer_le));

                    int32_t pwm = static_cast<int32_t>(le32toh(pwm_le));
                    int32_t steer = static_cast<int32_t>(le32toh(steer_le));

                    RCLCPP_INFO(this->get_logger(), "Received -> PWM: %d, Steer: %d", pwm, steer);
                } else {
                    RCLCPP_WARN(this->get_logger(), "Received packet with invalid header.");
                }
            } else {
                RCLCPP_WARN(this->get_logger(), "Received packet with unexpected size: %zd", len);
            }
        }
    }

private:
    static constexpr char kHeader[4] = {'I', 'R', 'I', 'S'};
    static constexpr size_t kPacketSize = 12;

    int sock_ = -1;
    int port_;
};

std::atomic<bool> g_running(true);

void on_sigint(int) {
    g_running = false;
}

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    std::signal(SIGINT, on_sigint);

    int port = argc > 1 ? std::atoi(argv[1]) : 12151;

    try {
        auto receiver = std::make_shared<UDPReceiver>(port);
        receiver->start_listening(g_running);
    } catch (const std::exception &e) {
        RCLCPP_ERROR(rclcpp::get_logger("main"), "Error: %s", e.what());
    }

    rclcpp::shutdown();
    return 0;
}