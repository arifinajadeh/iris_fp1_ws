
#include <arpa/inet.h>
#include <endian.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <functional>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float32_multi_array.hpp"
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

        // PID: Kp, Ki, Kd, batas output.
        pid_ = std::make_unique<PIDController>(
            20.0, 0.0, 0.0, 20.0
        );

        points_sub_ =
            create_subscription<std_msgs::msg::Float32MultiArray>(
                "/vision/center_points",
                10,
                std::bind(
                    &MasterNode::points_callback,
                    this,
                    std::placeholders::_1
                )
            );

        timer_ = create_wall_timer(
            std::chrono::milliseconds(50),
            std::bind(&MasterNode::control_loop, this)
        );

        RCLCPP_INFO(
            get_logger(),
            "Master siap. Menunggu titik dari BEV."
        );
    }

    ~MasterNode() override {
        if (sock_ >= 0) {
            send_packet(0, 0);
            close(sock_);
        }
    }

private:
    // Parameter PWM
    int32_t pwm_current_ = 0;

    static constexpr int32_t PWM_MAX = 150;
    static constexpr int32_t PWM_MIN = 80;
    static constexpr int32_t PWM_STEP = 5;

    // Timeout data BEV
    static constexpr int32_t DATA_TIMEOUT_MS = 1000;

    int sock_ = -1;
    sockaddr_in addr_{};

    std::unique_ptr<PIDController> pid_;

    rclcpp::Subscription<
        std_msgs::msg::Float32MultiArray
    >::SharedPtr points_sub_;

    rclcpp::TimerBase::SharedPtr timer_;

    std::vector<float> points_;
    bool points_received_ = false;

    std::chrono::steady_clock::time_point last_points_time_{};

    static constexpr double BEV_WIDTH = 800.0;

    void points_callback(
        const std_msgs::msg::Float32MultiArray::SharedPtr msg
    ) {
        if (msg->data.size() < 2 ||
            msg->data.size() % 2 != 0) {
            points_received_ = false;
            pid_->reset();
            return;
        }

        points_ = msg->data;
        points_received_ = true;

        last_points_time_ = std::chrono::steady_clock::now();
    }

    void control_loop() {
        // Periksa apakah data BEV masih diperbarui.
        const auto now = std::chrono::steady_clock::now();

        const bool data_timeout =
            points_received_ &&
            (now - last_points_time_ >
             std::chrono::milliseconds(DATA_TIMEOUT_MS));

        // Berhenti jika data BEV tidak tersedia atau kedaluwarsa.
        if (!points_received_ || data_timeout) {
            points_received_ = false;
            pwm_current_ = 0;

            pid_->reset();
            send_packet(0, 0);
            return;
        }

        const size_t jumlah_titik = points_.size() / 2;

        // Pilih titik ketiga, atau titik terakhir jika kurang dari 3.
        const size_t target_index =
            std::min<size_t>(2, jumlah_titik - 1);

        const double pixel_x = points_[2 * target_index];
        const double pixel_y = points_[2 * target_index + 1];

        // Hitung error posisi relatif terhadap tengah BEV.
        const double error =
            (pixel_x - BEV_WIDTH / 2.0) /
            (BEV_WIDTH / 2.0);

        // Hitung steering menggunakan PID.
        const double steering_output =
            pid_->update(error, 0.05);

        const int32_t steer_cmd =
            static_cast<int32_t>(
                std::lround(steering_output)
            );

        // Hitung PWM target berdasarkan besar belokan.
        const double steering_magnitude =
            std::abs(steer_cmd);

        const int32_t pwm_target =
            static_cast<int32_t>(
                std::clamp(
                    150.0 - 3.5 * steering_magnitude,
                    static_cast<double>(PWM_MIN),
                    static_cast<double>(PWM_MAX)
                )
            );

        // Soft start dan perubahan PWM bertahap.
        if (pwm_current_ < pwm_target) {
            pwm_current_ = std::min(
                pwm_current_ + PWM_STEP,
                pwm_target
            );
        }
        else if (pwm_current_ > pwm_target) {
            pwm_current_ = std::max(
                pwm_current_ - PWM_STEP,
                pwm_target
            );
        }

        // Kirim perintah ke simulator.
        send_packet(pwm_current_, steer_cmd);

        RCLCPP_INFO_THROTTLE(
            get_logger(),
            *get_clock(),
            1000,
            "Target: (%.1f, %.1f) | Error: %.3f | "
            "Steering: %d | PWM target: %d | PWM actual: %d",
            pixel_x,
            pixel_y,
            error,
            steer_cmd,
            pwm_target,
            pwm_current_
        );
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
            sock_,
            packet,
            sizeof(packet),
            0,
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
            "Error: %s",
            e.what()
        );
    }

    rclcpp::shutdown();
    return 0;
}

// #include <arpa/inet.h>
// #include <endian.h>
// #include <netinet/in.h>
// #include <sys/socket.h>
// #include <unistd.h>

// #include <algorithm>
// #include <chrono>
// #include <cmath>
// #include <cstdint>
// #include <cstring>
// #include <memory>
// #include <stdexcept>
// #include <functional>
// #include <vector>

// #include "rclcpp/rclcpp.hpp"
// #include "std_msgs/msg/float32_multi_array.hpp"
// #include "kinematika.hpp"

// class MasterNode : public rclcpp::Node {
// public:
//     MasterNode() : Node("master_node") {
//         sock_ = socket(AF_INET, SOCK_DGRAM, 0);

//         if (sock_ < 0) {
//             throw std::runtime_error("Gagal membuat UDP socket");
//         }

//         addr_.sin_family = AF_INET;
//         addr_.sin_port = htons(12151);

//         if (inet_pton(AF_INET, "127.0.0.1",
//                       &addr_.sin_addr) != 1) {
//             close(sock_);
//             sock_ = -1;
//             throw std::runtime_error("Alamat IP tidak valid");
//         }

//         // PID: Kp, Ki, Kd, batas output.
//         // Nilai ini masih untuk pengujian awal.
//         pid_ = std::make_unique<PIDController>(
//             20.0, 0.0, 0.0, 20.0
//         );

//         points_sub_ =
//             create_subscription<std_msgs::msg::Float32MultiArray>(
//                 "/vision/center_points",
//                 10,
//                 std::bind(
//                     &MasterNode::points_callback,
//                     this,
//                     std::placeholders::_1
//                 )
//             );

//         timer_ = create_wall_timer(
//             std::chrono::milliseconds(50),
//             std::bind(&MasterNode::control_loop, this)
//         );

//         RCLCPP_INFO(
//             get_logger(),
//             "Master siap. Menunggu titik dari BEV."
//         );
//     }

//     ~MasterNode() override {
//         if (sock_ >= 0) {
//             send_packet(0, 0);
//             close(sock_);
//         }
//     }

// private:
//     // int32_t pwm_current_ = 0;

//     // static constexpr int32_t PWM_MAX = 150;
//     // static constexpr int32_t PWM_MIN = 80;
//     // static constexpr int32_t PWM_STEP = 5;
    
//     int sock_ = -1;
//     sockaddr_in addr_{};

//     std::unique_ptr<PIDController> pid_;

//     rclcpp::Subscription<
//         std_msgs::msg::Float32MultiArray
//     >::SharedPtr points_sub_;

//     rclcpp::TimerBase::SharedPtr timer_;

//     std::vector<float> points_;
//     bool points_received_ = false;

//     static constexpr double BEV_WIDTH = 800.0;

//     void points_callback(
//         const std_msgs::msg::Float32MultiArray::SharedPtr msg
//     ) {
//         if (msg->data.size() < 2 ||
//             msg->data.size() % 2 != 0) {
//             points_received_ = false;
//             pid_->reset();
//             return;
//         }

//         points_ = msg->data;
//         points_received_ = true;
//     }

//     void control_loop() {
//         if (!points_received_) {
//             pid_->reset();
//             send_packet(0, 0);
//             return;
//         }

//         const size_t jumlah_titik = points_.size() / 2;

//         // Pilih titik ketiga, atau titik terakhir jika kurang dari 3.
//         const size_t target_index =
//             std::min<size_t>(2, jumlah_titik - 1);

//         const double pixel_x = points_[2 * target_index];
//         const double pixel_y = points_[2 * target_index + 1];

//         // Error posisi relatif terhadap tengah horizontal BEV.
//         // Rentang ideal: -1 sampai +1.
//         const double error =
//             (pixel_x - BEV_WIDTH / 2.0) /
//             (BEV_WIDTH / 2.0);

//         // Timer berjalan setiap 50 ms.
//         const double steering_output =
//             pid_->update(error, 0.05);

//         const int32_t steer_cmd =
//             static_cast<int32_t>(
//                 std::lround(steering_output)
//             );

//         RCLCPP_INFO_THROTTLE(
//             get_logger(),
//             *get_clock(),
//             1000,
//             "Target BEV: (%.1f, %.1f), error: %.3f, PID output: %.2f, command: %d",
//             pixel_x,
//             pixel_y,
//             error,
//             steering_output,
//             steer_cmd
//         );

//         // PWM tetap 0 selama pengujian steering.
//         send_packet(150, steer_cmd);
//     }


//     void send_packet(int32_t pwm, int32_t steer) {
//         uint8_t packet[12];
//         const char header[4] = {'I', 'R', 'I', 'S'};

//         const int32_t safe_pwm =
//             std::clamp<int32_t>(pwm, -255, 255);

//         const int32_t safe_steer =
//             std::clamp<int32_t>(steer, -90, 90);

//         const uint32_t pwm_le =
//             htole32(static_cast<uint32_t>(safe_pwm));

//         const uint32_t steer_le =
//             htole32(static_cast<uint32_t>(safe_steer));

//         std::memcpy(packet, header, 4);
//         std::memcpy(packet + 4, &pwm_le, 4);
//         std::memcpy(packet + 8, &steer_le, 4);

//         const auto sent = sendto(
//             sock_,
//             packet,
//             sizeof(packet),
//             0,
//             reinterpret_cast<sockaddr*>(&addr_),
//             sizeof(addr_)
//         );

//         if (sent != static_cast<ssize_t>(sizeof(packet))) {
//             RCLCPP_WARN(
//                 get_logger(),
//                 "Pengiriman UDP gagal atau paket tidak lengkap"
//             );
//         }
//     }
// };

// int main(int argc, char * argv[]) {
//     rclcpp::init(argc, argv);

//     try {
//         auto node = std::make_shared<MasterNode>();
//         rclcpp::spin(node);
//     } catch (const std::exception &e) {
//         RCLCPP_ERROR(
//             rclcpp::get_logger("master_node"),
//             "Error: %s",
//             e.what()
//         );
//     }

//     rclcpp::shutdown();
//     return 0;
// }
