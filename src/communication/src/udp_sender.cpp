// #include <arpa/inet.h>
// #include <endian.h>
// #include <netinet/in.h>
// #include <sys/socket.h>
// #include <unistd.h>
// #include <rclcpp/rclcpp.hpp>

// #include <algorithm>
// #include <chrono>
// #include <csignal>
// #include <cstdint>
// #include <cstdio>
// #include <cstdlib>
// #include <cstring>
// #include <thread>
// #include <atomic>
// #include <stdexcept>
// #include <string>

// using namespace std::chrono_literals;

// class UDPSender : public rclcpp::Node {
// public:
//     UDPSender(int32_t pwm, int32_t steer, const std::string& host, int port)
//         : Node("udp_sender_node"), pwm_(pwm), steer_(steer), host_(host), port_(port) {
        
//         sock_ = socket(AF_INET, SOCK_DGRAM, 0);
//         if (sock_ < 0) {
//             std::perror("socket");
//             throw std::runtime_error("Failed to create UDP socket.");
//         }

//         addr_.sin_family = AF_INET;
//         addr_.sin_port = htons(static_cast<uint16_t>(port_));
//         if (inet_pton(AF_INET, host_.c_str(), &addr_.sin_addr) != 1) {
//             close(sock_);
//             throw std::runtime_error("Invalid host address: " + host_);
//         }
//     }

//     ~UDPSender() {
//         // Kirim paket berhenti (pwm = 0) saat shutdown
//         uint8_t packet[kPacketSize];
//         build_packet(packet, 0, steer_);
//         sendto(sock_, packet, sizeof(packet), 0,
//                reinterpret_cast<sockaddr *>(&addr_), sizeof(addr_));

//         if (sock_ >= 0) {
//             close(sock_);
//         }
//         RCLCPP_INFO(this->get_logger(), "UDPSender stopped and socket closed.");
//     }

//     void run(std::atomic<bool>& running_flag) {
//         uint8_t packet[kPacketSize];
//         build_packet(packet, pwm_, steer_);

//         int32_t clamped_pwm = std::clamp<int32_t>(pwm_, -255, 255);
//         std::printf("Sending pwm=%d steer=%d to %s:%d at 20 Hz (Ctrl-C to stop)\n",
//                     clamped_pwm, steer_, host_.c_str(), port_);

//         while (running_flag && rclcpp::ok()) {
//             if (sendto(sock_, packet, sizeof(packet), 0,
//                        reinterpret_cast<sockaddr *>(&addr_), sizeof(addr_)) < 0) {
//                 std::perror("sendto");
//             }
//             std::this_thread::sleep_for(kPeriod);
//         }
//     }

// private:
//     static constexpr char kHeader[4] = {'I', 'R', 'I', 'S'};
//     static constexpr size_t kPacketSize = 12;
//     static constexpr auto kPeriod = std::chrono::milliseconds(50);  // 20 Hz

//     int sock_ = -1;
//     sockaddr_in addr_{};

//     int32_t pwm_ = 0;
//     int32_t steer_ = 0;
//     std::string host_;
//     int port_;

//     void build_packet(uint8_t (&buf)[kPacketSize], int32_t pwm, int32_t steer) {
//         int32_t clamped_pwm = std::clamp<int32_t>(pwm, -255, 255);
//         uint32_t pwm_le = htole32(static_cast<uint32_t>(clamped_pwm));
//         uint32_t steer_le = htole32(static_cast<uint32_t>(steer));

//         std::memcpy(buf, kHeader, sizeof(kHeader));
//         std::memcpy(buf + 4, &pwm_le, sizeof(pwm_le));
//         std::memcpy(buf + 8, &steer_le, sizeof(steer_le));
//     }
// };

// // Global atomic flag untuk penanganan signal
// std::atomic<bool> g_running(true);

// void on_sigint(int) {
//     g_running = false;
// }

// int main(int argc, char **argv) {
//     if (argc < 2) {
//         std::fprintf(stderr, "Usage: %s <pwm> [steer_deg] [host] [port]\n", argv[0]);
//         return 1;
//     }

//     int32_t pwm = std::atoi(argv[1]);
//     int32_t steer = argc > 2 ? std::atoi(argv[2]) : 0;
//     std::string host = argc > 3 ? argv[3] : "127.0.0.1";
//     int port = argc > 4 ? std::atoi(argv[4]) : 12151;

//     rclcpp::init(argc, argv);
//     std::signal(SIGINT, on_sigint);

//     try {
//         auto sender = std::make_shared<UDPSender>(pwm, steer, host, port);
//         sender->run(g_running);
//     } catch (const std::exception &e) {
//         RCLCPP_ERROR(rclcpp::get_logger("main"), "Error: %s", e.what());
//     }

//     rclcpp::shutdown();
//     return 0;
// }



#include <arpa/inet.h>
#include <endian.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#include <rclcpp/rclcpp.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

using namespace std::chrono_literals;

class UDPSender : public rclcpp::Node {
public:
    UDPSender()
        : Node("udp_sender_node") {
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);

        if (sock_ < 0) {
            throw std::runtime_error("Gagal membuat UDP socket");
        }

        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(12151);

        if (inet_pton(AF_INET, "127.0.0.1",
                      &addr_.sin_addr) != 1) {
            close(sock_);
            sock_ = -1;
            throw std::runtime_error("Alamat IP tidak valid");
        }
    }

    ~UDPSender() {
        if (sock_ >= 0) {
            send_packet(0, steer_);
            close(sock_);
        }
    }

    void run() {
        std::printf(
            "\n=== KONTROL MANUAL MOBIL ===\n"
            "W     : Maju\n"
            "S     : Mundur\n"
            "A     : Belok kiri\n"
            "D     : Belok kanan\n"
            "X     : Roda kembali lurus\n"
            "SPACE : Berhenti total\n"
            "Q     : Keluar\n\n"
            "Tekan tombol di terminal ini.\n"
        );

        struct termios old_terminal{};
        struct termios new_terminal{};

        if (tcgetattr(STDIN_FILENO, &old_terminal) < 0) {
            throw std::runtime_error(
                "Jalankan program dari terminal interaktif"
            );
        }

        new_terminal = old_terminal;
        new_terminal.c_lflag &= ~(ICANON | ECHO);
        new_terminal.c_cc[VMIN] = 0;
        new_terminal.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSANOW,
                      &new_terminal) < 0) {
            throw std::runtime_error(
                "Gagal mengaktifkan kontrol keyboard"
            );
        }

        bool running = true;
        char key;

        while (running && rclcpp::ok()) {
            // Baca tombol yang tersedia tanpa menunggu Enter.
            while (read(STDIN_FILENO, &key, 1) == 1) {
                switch (key) {
                    case 'w':
                    case 'W':
                        pwm_ = 120;
                        break;

                    case 's':
                    case 'S':
                        pwm_ = -120;
                        break;

                    case 'a':
                    case 'A':
                        steer_ = -20;
                        break;

                    case 'd':
                    case 'D':
                        steer_ = 20;
                        break;

                    case 'x':
                    case 'X':
                        steer_ = 0;
                        break;

                    case ' ':
                        pwm_ = 0;
                        steer_ = 0;
                        break;

                    case 'q':
                    case 'Q':
                        pwm_ = 0;
                        running = false;
                        break;

                    default:
                        break;
                }
            }

            send_packet(pwm_, steer_);

            std::this_thread::sleep_for(50ms);
        }

        // Kirim perintah berhenti sebelum keluar.
        send_packet(0, 0);

        // Pulihkan pengaturan terminal.
        tcsetattr(STDIN_FILENO, TCSANOW, &old_terminal);

        std::printf("\nKontrol manual dihentikan.\n");
    }

private:
    int sock_ = -1;
    sockaddr_in addr_{};

    int32_t pwm_ = 0;
    int32_t steer_ = 0;

    void send_packet(int32_t pwm, int32_t steer) {
        uint8_t packet[12];
        const char header[4] = {'I', 'R', 'I', 'S'};

        int32_t safe_pwm =
            std::clamp<int32_t>(pwm, -255, 255);

        uint32_t pwm_le =
            htole32(static_cast<uint32_t>(safe_pwm));

        uint32_t steer_le =
            htole32(static_cast<uint32_t>(steer));

        std::memcpy(packet, header, 4);
        std::memcpy(packet + 4, &pwm_le, 4);
        std::memcpy(packet + 8, &steer_le, 4);

        sendto(
            sock_, packet, sizeof(packet), 0,
            reinterpret_cast<sockaddr*>(&addr_),
            sizeof(addr_)
        );
    }
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);

    try {
        auto sender = std::make_shared<UDPSender>();
        sender->run();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
    }

    rclcpp::shutdown();
    return 0;
}
