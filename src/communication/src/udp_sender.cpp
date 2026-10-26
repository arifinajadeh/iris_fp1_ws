#include <arpa/inet.h>
#include <endian.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <rclcpp/rclcpp.hpp>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <atomic>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;

class UDPSender : public rclcpp::Node {
public:
    UDPSender(int32_t pwm, int32_t steer, const std::string& host, int port)
        : Node("udp_sender_node"), pwm_(pwm), steer_(steer), host_(host), port_(port) {
        
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0) {
            std::perror("socket");
            throw std::runtime_error("Failed to create UDP socket.");
        }

        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(static_cast<uint16_t>(port_));
        if (inet_pton(AF_INET, host_.c_str(), &addr_.sin_addr) != 1) {
            close(sock_);
            throw std::runtime_error("Invalid host address: " + host_);
        }
    }

    ~UDPSender() {
        // Kirim paket berhenti (pwm = 0) saat shutdown
        uint8_t packet[kPacketSize];
        build_packet(packet, 0, steer_);
        sendto(sock_, packet, sizeof(packet), 0,
               reinterpret_cast<sockaddr *>(&addr_), sizeof(addr_));

        if (sock_ >= 0) {
            close(sock_);
        }
        RCLCPP_INFO(this->get_logger(), "UDPSender stopped and socket closed.");
    }

    void run(std::atomic<bool>& running_flag) {
        uint8_t packet[kPacketSize];
        build_packet(packet, pwm_, steer_);

        int32_t clamped_pwm = std::clamp<int32_t>(pwm_, -255, 255);
        std::printf("Sending pwm=%d steer=%d to %s:%d at 20 Hz (Ctrl-C to stop)\n",
                    clamped_pwm, steer_, host_.c_str(), port_);

        while (running_flag && rclcpp::ok()) {
            if (sendto(sock_, packet, sizeof(packet), 0,
                       reinterpret_cast<sockaddr *>(&addr_), sizeof(addr_)) < 0) {
                std::perror("sendto");
            }
            std::this_thread::sleep_for(kPeriod);
        }
    }

private:
    static constexpr char kHeader[4] = {'I', 'R', 'I', 'S'};
    static constexpr size_t kPacketSize = 12;
    static constexpr auto kPeriod = std::chrono::milliseconds(50);  // 20 Hz

    int sock_ = -1;
    sockaddr_in addr_{};

    int32_t pwm_ = 0;
    int32_t steer_ = 0;
    std::string host_;
    int port_;

    void build_packet(uint8_t (&buf)[kPacketSize], int32_t pwm, int32_t steer) {
        int32_t clamped_pwm = std::clamp<int32_t>(pwm, -255, 255);
        uint32_t pwm_le = htole32(static_cast<uint32_t>(clamped_pwm));
        uint32_t steer_le = htole32(static_cast<uint32_t>(steer));

        std::memcpy(buf, kHeader, sizeof(kHeader));
        std::memcpy(buf + 4, &pwm_le, sizeof(pwm_le));
        std::memcpy(buf + 8, &steer_le, sizeof(steer_le));
    }
};

// Global atomic flag untuk penanganan signal
std::atomic<bool> g_running(true);

void on_sigint(int) {
    g_running = false;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <pwm> [steer_deg] [host] [port]\n", argv[0]);
        return 1;
    }

    int32_t pwm = std::atoi(argv[1]);
    int32_t steer = argc > 2 ? std::atoi(argv[2]) : 0;
    std::string host = argc > 3 ? argv[3] : "127.0.0.1";
    int port = argc > 4 ? std::atoi(argv[4]) : 12151;

    rclcpp::init(argc, argv);
    std::signal(SIGINT, on_sigint);

    try {
        auto sender = std::make_shared<UDPSender>(pwm, steer, host, port);
        sender->run(g_running);
    } catch (const std::exception &e) {
        RCLCPP_ERROR(rclcpp::get_logger("main"), "Error: %s", e.what());
    }

    rclcpp::shutdown();
    return 0;
}