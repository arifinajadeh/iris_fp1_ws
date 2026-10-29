
#include <memory>
#include <functional>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/image_encodings.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>

using namespace cv;

class BEVNode : public rclcpp::Node
{
public:
    BEVNode() : Node("bev_node")
    {
        // namedWindow("Control", WINDOW_AUTOSIZE);

        // createTrackbar("Hmin", "Control", &h_min_, 179);
        // createTrackbar("Hmax", "Control", &h_max_, 179);
        // createTrackbar("Smin", "Control", &s_min_, 255);
        // createTrackbar("Smax", "Control", &s_max_, 255);
        // createTrackbar("Vmin", "Control", &v_min_, 255);
        // createTrackbar("Vmax", "Control", &v_max_, 255);

        sub_ = create_subscription<sensor_msgs::msg::Image>(
            "/catvehicle/camera_front/image_raw_front",
            10,
            std::bind(
                &BEVNode::imageCallback,
                this,
                std::placeholders::_1
            )
        );

        RCLCPP_INFO(get_logger(), "Node BEV berjalan");
    }

private:
    // int h_min_ = 0;
    // int h_max_ = 179;
    // int s_min_ = 0;
    // int s_max_ = 255;
    // int v_min_ = 0;
    // int v_max_ = 255;

    void imageCallback(
        sensor_msgs::msg::Image::ConstSharedPtr msg)
    {
        try
        {
            Mat frame = cv_bridge::toCvCopy(
                msg, sensor_msgs::image_encodings::BGR8
            )->image;

            int w = frame.cols;
            int height = frame.rows;

            std::vector<cv::Point2f> srcPoints = {
                {1, 579},    // kiri bawah
                {287, 461},  // kiri atas
                {653, 469},  // kanan atas
                {795, 604}   // kanan bawah
            };
            
            std::vector<cv::Point2f> dstPoints = {
                {0, 600},
                {0, 0},
                {800, 0},
                {800, 600}
            };
            Mat transform = getPerspectiveTransform(
                srcPoints.data(), dstPoints.data()
            );

            // Ukuran output BEV: 800 x 600
            Mat bev;
            warpPerspective(
                frame,
                bev,
                transform,
                Size(800, 600)
            );

            // Threshold HSV
            Mat frame_hsv, frame_threshold;

            cvtColor(bev, frame_hsv, COLOR_BGR2HSV);

            inRange(
                frame_hsv,
                Scalar(0, 18, 0),
                Scalar(103, 255, 255),
                frame_threshold
            );

            imshow("BEV", bev);
            imshow("Threshold", frame_threshold);
            waitKey(1);
        }
        catch (const cv_bridge::Exception &e)
        {
            RCLCPP_WARN(
                get_logger(), "Gagal konversi gambar: %s", e.what()
            );
        }
        catch (const cv::Exception &e)
        {
            RCLCPP_WARN(
                get_logger(), "Gagal memproses gambar: %s", e.what()
            );
        }
    }

    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<BEVNode>());
    cv::destroyAllWindows();
    rclcpp::shutdown();
    return 0;
}
