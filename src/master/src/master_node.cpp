
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "kinematika.hpp"

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);


    auto node = std::make_shared<rclcpp::Node>("master_node");

    AckermannKinematics kinematics(2.82);

    double target_x = 1.0;
    double target_y = -0.5;

    double steering = kinematics.hitungSteering(target_x, target_y);

    RCLCPP_INFO(
        node->get_logger(),
        "Target (x=%.2f, y=%.2f), sudut steering=%.2f derajat",
        target_x,
        target_y,
        steering
    );

    // Menjalankan node ROS 2
    rclcpp::spin(node);

    rclcpp::shutdown();
    return 0;
}
