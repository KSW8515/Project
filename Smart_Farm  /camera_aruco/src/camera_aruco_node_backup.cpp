#include <chrono>
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

class CameraArucoNode : public rclcpp::Node {
public:
    CameraArucoNode() : Node("camera_aruco_node") {
        cap = std::make_shared<cv::VideoCapture>(0);
        if (!cap->isOpened()) {
            RCLCPP_ERROR(this->get_logger(), "Failed to open camera.");
            rclcpp::shutdown();
            return;
        }

        publisher_ = this->create_publisher<sensor_msgs::msg::Image>("aruco_image", 10);
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(100),
            std::bind(&CameraArucoNode::timer_callback, this)
        );
    }
private:
    void timer_callback() {
        static int frame_count = 0;
        cv::Mat frame;
        if (!cap->read(frame)) {
            RCLCPP_ERROR(this->get_logger(), "Failed to capture image.");
            return;
        }
        frame_count++;
        if (frame_count % 3 == 0) {
            cv::aruco::detectMarkers(frame, dictionary, markerCorners, markerIds);
            if (!markerIds.empty()) {
                cv::aruco::drawDetectedMarkers(frame, markerCorners, markerIds);
            }
            cv::Point2f markerCenter;
            for (size_t i = 0; i < markerIds.size(); ++i) {
                cv::Moments mu = cv::moments(markerCorners[i]);
                markerCenter = cv::Point2f(mu.m10 / mu.m00, mu.m01 / mu.m00);
                cv::putText(frame, std::to_string(markerIds[i]), markerCenter, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
            }
        }
        auto msg = cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", frame).toImageMsg();
        publisher_->publish(*msg);
    }
    cv::Ptr<cv::VideoCapture> cap;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;

    std::vector<int> markerIds;
    std::vector<std::vector<cv::Point2f>> markerCorners;
    cv::Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250);

};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<CameraArucoNode>());
    rclcpp::shutdown();
    return 0;
}
