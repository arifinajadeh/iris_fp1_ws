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
