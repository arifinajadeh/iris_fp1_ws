
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <algorithm>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/image_encodings.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>
#include <std_msgs/msg/float32_multi_array.hpp>

using namespace cv;

class BEVNode : public rclcpp::Node
{
public:
    BEVNode() : Node("bev_node")
    {
        // Publisher untuk mengirim titik tengah garis
        points_pub_ =
            create_publisher<std_msgs::msg::Float32MultiArray>(
                "/vision/center_points", 10
            );

        // Subscriber kamera
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
    rclcpp::Publisher<
        std_msgs::msg::Float32MultiArray
    >::SharedPtr points_pub_;

    rclcpp::Subscription<
        sensor_msgs::msg::Image
    >::SharedPtr sub_;

    void imageCallback(
        sensor_msgs::msg::Image::ConstSharedPtr msg)
    {
        try
        {
            Mat frame = cv_bridge::toCvCopy(
                msg,
                sensor_msgs::image_encodings::BGR8

            std::vector<cv::Point2f> srcPoints = {
                {9, 527},
                {344, 444},
                {656, 450},
                {795, 597}
            };

            std::vector<cv::Point2f> dstPoints = {
                {0, 600},
                {0, 0},
                {800, 0},
                {800, 600}
            };

            Mat transform = getPerspectiveTransform(
                srcPoints.data(),
                dstPoints.data()
            );

            Mat bev;

            warpPerspective(
                frame,
                bev,
                transform,
                Size(800, 600)
            );


            Mat frame_hsv;
            Mat frame_threshold;

            cvtColor(
                bev,
                frame_hsv,
                COLOR_BGR2HSV
            );

            inRange(
                frame_hsv,
                Scalar(0, 0, 150),
                Scalar(180, 70, 255),
                frame_threshold
            );

            Mat frame_morph = frame_threshold.clone();

            erode(
                frame_morph,
                frame_morph,
                Mat(),
                Point(-1, -1),
                6
            );

            dilate(
                frame_morph,
                frame_morph,
                Mat(),
                Point(-1, -1),
                7
            );

            Mat contour_input = frame_morph.clone();

            std::vector<std::vector<cv::Point>> contours;
            std::vector<cv::Vec4i> hierarchy;

            findContours(
                contour_input,
                contours,
                hierarchy,
                RETR_EXTERNAL,
                CHAIN_APPROX_SIMPLE
            );

            Mat result = bev.clone();

            // Menampung titik tengah
            std::vector<cv::Point> centers;

            for (size_t i = 0; i < contours.size(); i++)
            {
                double area = contourArea(contours[i]);

                if (area < 1000 || area > 5000)
                    continue;

                Rect box = boundingRect(contours[i]);

                int centerX = box.x + box.width / 2;
                int centerY = box.y + box.height / 2;

                centers.push_back(
                    Point(centerX, centerY)
                );

                drawContours(
                    result,
                    contours,
                    static_cast<int>(i),
                    Scalar(0, 255, 0),
                    2
                );

                circle(
                    result,
                    Point(centerX, centerY),
                    4,
                    Scalar(0, 0, 255),
                    FILLED
                );

                putText(
                    result,
                    "(" + std::to_string(centerX) + "," +
                    std::to_string(centerY) + ")",
                    Point(centerX + 5, centerY - 5),
                    FONT_HERSHEY_SIMPLEX,
                    0.4,
                    Scalar(255, 0, 0),
                    1
                );
            }


            std::sort(
                centers.begin(),
                centers.end(),
                [](const cv::Point& a, const cv::Point& b) {
                    return a.y > b.y;
                }
            );

            for (size_t i = 0; i + 1 < centers.size(); i++)
            {
                line(
                    result,
                    centers[i],
                    centers[i + 1],
                    Scalar(255, 0, 255),
                    3
                );
            }


            std_msgs::msg::Float32MultiArray points_msg;

            for (const auto& p : centers)
            {
                points_msg.data.push_back(
                    static_cast<float>(p.x)
                );

                points_msg.data.push_back(
                    static_cast<float>(p.y)
                );
            }

            points_pub_->publish(points_msg);

            imshow("Mentah Lohya", frame);
            imshow("BEV", bev);
            imshow("Threshold", frame_threshold);
            imshow("Morph", frame_morph);
            imshow("Contour and Center", result);

            waitKey(1);
        }
        catch (const cv_bridge::Exception &e)
        {
            RCLCPP_WARN(
                get_logger(),
                "Gagal konversi gambar: %s",
                e.what()
            );
        }
        catch (const cv::Exception &e)
        {
            RCLCPP_WARN(
                get_logger(),
                "Gagal memproses gambar: %s",
                e.what()
            );
        }
    }
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);

    rclcpp::spin(
        std::make_shared<BEVNode>()
    );

    cv::destroyAllWindows();

    rclcpp::shutdown();

    return 0;
}
