
#include <chrono>
#include <memory>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
//#include <opencv2/objdetect/aruco_detector.hpp>
#include <cv_bridge/cv_bridge.h>
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "geometry_msgs/msg/twist.hpp"

class CameraArucoNode : public rclcpp::Node {
public:
    CameraArucoNode() : Node("camera_aruco_node") {
        //
        cap_ = std::make_shared<cv::VideoCapture>(0);
        if (!cap_->isOpened()) {
            RCLCPP_ERROR(this->get_logger(), "Failed to open camera.");
            rclcpp::shutdown();
            return;
        }

        //
        image_pub_ = this->create_publisher<sensor_msgs::msg::Image>("aruco_image", 10);
        cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);

        //ArUco 
        dictionary_ = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_1000);
        //detectorParams_ = cv::aruco::DetectorParameters();
        detectorParams_ = cv::aruco::DetectorParameters::create();
        
		detector_ = cv::aruco::ArucoDetector(dictionary_, detectorParams_);

        //YAML 
        load_camera_params("/home/ubuntu/.ros/camera_info/mmal_service_16.1.yaml");

        //100ms 
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(100),
            std::bind(&CameraArucoNode::timer_callback, this)
        );
    }

private:
    void load_camera_params(std::string yaml_path) {
        cv::FileStorage fs(yaml_path, cv::FileStorage::READ);
        if (fs.isOpened()) {
            cv::FileNode cm_node = fs["camera_matrix"];
            std::vector<double> cm_data;
            cm_node["data"] >> cm_data;
            cameraMatrix_ = cv::Mat((int)cm_node["rows"], (int)cm_node["cols"], CV_64F, cm_data.data()).clone();

            cv::FileNode dc_node = fs["distortion_coefficients"];
            std::vector<double> dc_data;
            dc_node["data"] >> dc_data;
            distCoeffs_ = cv::Mat((int)dc_node["rows"], (int)dc_node["cols"], CV_64F, dc_data.data()).clone();
            
            RCLCPP_INFO(this->get_logger(), "Camera parameters loaded successfully!");
        } else {
            RCLCPP_WARN(this->get_logger(), "Failed to open YAML! Using default values.");
            cameraMatrix_ = (cv::Mat_<double>(3,3) << 524.64, 0, 324.62, 0, 523.68, 242.01, 0, 0, 1);
            distCoeffs_ = (cv::Mat_<double>(1,5) << 0.099, -0.26, -0.008, 0.016, 0.0);
        }
    }

    void timer_callback() {
        cv::Mat frame;
        if (!cap_->read(frame)) return;

        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners;
        //detector_.detectMarkers(frame, corners, ids);
		cv::aruco::detectMarkers(frame, dictionary_, corners, ids, detectorParams_);


        geometry_msgs::msg::Twist twist_msg;
        float markerLength = 0.05; // 5cm

        if (!ids.empty()) {
            cv::aruco::drawDetectedMarkers(frame, corners, ids);

            for (size_t i = 0; i < ids.size(); ++i) {
                // solvePnP
                cv::Mat objPoints(4, 1, CV_32FC3);
                objPoints.ptr<cv::Vec3f>(0)[0] = cv::Vec3f(-markerLength/2.f, markerLength/2.f, 0);
                objPoints.ptr<cv::Vec3f>(0)[1] = cv::Vec3f(markerLength/2.f, markerLength/2.f, 0);
                objPoints.ptr<cv::Vec3f>(0)[2] = cv::Vec3f(markerLength/2.f, -markerLength/2.f, 0);
                objPoints.ptr<cv::Vec3f>(0)[3] = cv::Vec3f(-markerLength/2.f, -markerLength/2.f, 0);

                cv::Vec3d rvec,tvec;    
                cv::solvePnP(objPoints, corners[i], cameraMatrix_, distCoeffs_, rvec, tvec);
                cv::drawFrameAxes(frame, cameraMatrix_, distCoeffs_, rvec, tvec, 0.03);

                //
                if (i == 0) {
                    double angle_x = tvec[0];
                    double distance = tvec[2];
                    double k_p = 0.5;

                    //
                    if (std::abs(angle_x) > 0.01) {
                        twist_msg.angular.z = -k_p * angle_x;
                    } else {
                        twist_msg.angular.z = 0.0;
                    }

                    //
                    if (distance < 0.3) {
                        twist_msg.linear.x = 0.0;
                    } else {
                        twist_msg.linear.x = 0.15;
                    }
                    RCLCPP_INFO(this->get_logger(), "ID: %d, Distance: %.2f, Angle: %.4f", ids[i], distance, angle_x);
                }
            }
        } else {
            //
            twist_msg.linear.x = 0.0;
            twist_msg.angular.z = 0.0;
        }

        //
        cmd_vel_pub_->publish(twist_msg);
        auto img_msg = cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", frame).toImageMsg();
        image_pub_->publish(*img_msg);
    }

    //
    std::shared_ptr<cv::VideoCapture> cap_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
    rclcpp::TimerBase::SharedPtr timer_;

    cv::aruco::Dictionary dictionary_;
    cv::aruco::DetectorParameters detectorParams_;
    cv::aruco::ArucoDetector detector_;
    cv::Mat cameraMatrix_, distCoeffs_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<CameraArucoNode>());
    rclcpp::shutdown();
    return 0;
}
