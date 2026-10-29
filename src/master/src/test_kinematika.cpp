
#include <iostream>
#include "kinematika.hpp"

int main() {
    AckermannKinematics kin(2.82);

    std::cout << "Target lurus: "
              << kin.hitungSteering(2, 0) << " derajat\n";

std::cout << "Kiri dekat: "
          << kin.hitungSteering(5, 0.5) << " derajat\n";

std::cout << "Kanan dekat: "
          << kin.hitungSteering(5, -0.5) << " derajat\n";   
    return 0;
}
