
#include <arpa/inet.h>
#include <endian.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>

#include "rclcpp/rclcpp.hpp"
#include "kinematika.hpp"

class MasterNode : public rclcpp::Node {
public:
    MasterNode() : Node("master_node") {
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

        // Wheelbase sementara; sesuaikan dengan model simulator.
        kinematics_ = std::make_unique<AckermannKinematics>(2.82);

        // Target relatif terhadap mobil.
        target_x_ = 5.0;
        target_y_ = 0.5;

        timer_ = create_wall_timer(
            std::chrono::milliseconds(50),
            std::bind(&MasterNode::control_loop, this)
        );

        RCLCPP_INFO(
            get_logger(),
            "Master siap. Target: x=%.2f, y=%.2f",
            target_x_, target_y_
        );
    }

    ~MasterNode() override {
        if (sock_ >= 0) {
            send_packet(0, 0);
            close(sock_);
        }
    }

private:
    int sock_ = -1;
    sockaddr_in addr_{};

    double target_x_ = 5.0;
    double target_y_ = 0.5;

    std::unique_ptr<AckermannKinematics> kinematics_;
    rclcpp::TimerBase::SharedPtr timer_;


    void control_loop() {
        // Target masih tetap, jadi ini hanya untuk menguji kontrol.
        const double steering =
            kinematics_->hitungSteering(target_x_, target_y_);

        const int32_t steer_cmd = static_cast<int32_t>(
            std::lround(-steering)
        );

        // Uji awal: hentikan mobil.
        constexpr int32_t pwm_cmd = 40;

        send_packet(pwm_cmd, steer_cmd);
    }



    void send_packet(int32_t pwm, int32_t steer) {
        uint8_t packet[12];
        const char header[4] = {'I', 'R', 'I', 'S'};

        const int32_t safe_pwm =
            std::clamp<int32_t>(pwm, -255, 255);
        const int32_t safe_steer =
            std::clamp<int32_t>(steer, -90, 90);

        const uint32_t pwm_le =
            htole32(static_cast<uint32_t>(safe_pwm));
        const uint32_t steer_le =
            htole32(static_cast<uint32_t>(safe_steer));

        std::memcpy(packet, header, 4);
        std::memcpy(packet + 4, &pwm_le, 4);
        std::memcpy(packet + 8, &steer_le, 4);

        const auto sent = sendto(
            sock_, packet, sizeof(packet), 0,
            reinterpret_cast<sockaddr*>(&addr_),
            sizeof(addr_)
        );

        if (sent != static_cast<ssize_t>(sizeof(packet))) {
            RCLCPP_WARN(
                get_logger(),
                "Pengiriman UDP gagal atau paket tidak lengkap"
            );
        }
    }
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);

    try {
        auto node = std::make_shared<MasterNode>();
        rclcpp::spin(node);
    } catch (const std::exception &e) {
        RCLCPP_ERROR(
            rclcpp::get_logger("master_node"),
            "Error: %s", e.what()
        );
    }

    rclcpp::shutdown();
    return 0;
}
