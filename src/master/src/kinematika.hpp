
#ifndef KINEMATIKA_HPP
#define KINEMATIKA_HPP

#include <cmath>
#include <stdexcept>
#include <algorithm>

class AckermannKinematics {
public:
    explicit AckermannKinematics(double wheelbase)
        : wheelbase_(wheelbase) {
        if (wheelbase_ <= 0.0) {
            throw std::invalid_argument(
                "Wheelbase harus lebih besar dari nol"
            );
        }
    }

    // x dan y adalah koordinat target relatif terhadap mobil.
    // x positif = ke depan, y positif = ke kiri.
    // Hasil berupa sudut steering dalam derajat.
    double hitungSteering(double x, double y) const {
        const double jarak = std::hypot(x, y);

        if (jarak < 1e-6) {
            return 0.0;
        }

        const double alpha = std::atan2(y, x);

        const double delta = std::atan2(
        2.0 * wheelbase_ * std::sin(alpha),
        jarak
    );
        constexpr double RAD_TO_DEG = 180.0 / 3.14159265358979323846;

        double steering = delta * RAD_TO_DEG;

        constexpr double MAX_STEERING_DEG = 0.6 * RAD_TO_DEG;

        if (steering > MAX_STEERING_DEG) {
            steering = MAX_STEERING_DEG;
        } else if (steering < -MAX_STEERING_DEG) {
            steering = -MAX_STEERING_DEG;
        }
        return steering;
    }

private:
    double wheelbase_;
};

class PIDController {
public:
    PIDController(double kp, double ki, double kd,
                  double max_output)
        : kp_(kp), ki_(ki), kd_(kd),
          max_output_(max_output) {}

    double update(double error, double dt) {
        if (dt <= 0.0) {
            return 0.0;
        }

        integral_ += error * dt;

        const double derivative =
            has_previous_error_
                ? (error - previous_error_) / dt
                : 0.0;

        previous_error_ = error;
        has_previous_error_ = true;

        double output =
            kp_ * error
            + ki_ * integral_
            + kd_ * derivative;

        output = std::clamp(
            output, -max_output_, max_output_);

        return output;
    }

    void reset() {
        integral_ = 0.0;
        previous_error_ = 0.0;
        has_previous_error_ = false;
    }

private:
    double kp_, ki_, kd_;
    double max_output_;

    double integral_ = 0.0;
    double previous_error_ = 0.0;
    bool has_previous_error_ = false;
};

#endif
