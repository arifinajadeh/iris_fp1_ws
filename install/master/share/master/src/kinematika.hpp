
#ifndef KINEMATIKA_HPP
#define KINEMATIKA_HPP

#include <cmath>
#include <stdexcept>

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

        return delta * RAD_TO_DEG;
    }

private:
    double wheelbase_;
};

#endif
