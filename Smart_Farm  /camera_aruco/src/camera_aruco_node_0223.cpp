#include <chrono>
#include <memory>
#include <vector>
#include <string>
#include <cmath>

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <cv_bridge/cv_bridge.h>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class CameraArucoNode : public rclcpp::Node {
public:
    enum class State { IDLE, MOVING, RETURN_1, RETURN_2, WAIT };

    CameraArucoNode() : Node("camera_aruco_node"), state_(State::IDLE) {
        cap_ = std::make_shared<cv::VideoCapture>(0);
        
		//publisher
        image_pub_ = this->create_publisher<sensor_msgs::msg::Image>("burger/aruco_image", 10);
        cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
        goal_pub_ = this->create_publisher<geometry_msgs::msg::PoseStamped>("goal_pose", 10);
        status_pub_ = this->create_publisher<std_msgs::msg::String>("work_status", 10);

		//subscription
        signal_sub_ = this->create_subscription<std_msgs::msg::String>(
            "qt_command", 10, std::bind(&CameraArucoNode::signal_callback, this, std::placeholders::_1));
        amcl_sub_ = this->create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
            "burger/amcl_pose", 10, std::bind(&CameraArucoNode::amcl_callback, this, std::placeholders::_1));

        dictionary_ = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_1000);
        detectorParams_ = cv::aruco::DetectorParameters::create();
        
        load_camera_params("/home/ubuntu/.ros/camera_info/mmal_service_16.1.yaml");

        timer_ = this->create_wall_timer(100ms, std::bind(&CameraArucoNode::timer_callback, this));
    }

private:
	bool first_pose_saved_ = false;
	
	void signal_callback(const std_msgs::msg::String::SharedPtr msg) {
	    std::string data = msg->data;
	    
	    if (data.find("burger/") == 0) {
	        data = data.substr(7);
	    }
	
	    data.erase(0, data.find_first_not_of(" \t\n\r"));
	    data.erase(data.find_last_not_of(" \t\n\r") + 1);
	
	    RCLCPP_INFO(this->get_logger(), "[%s]", data.c_str());
	
		if (data == "start") {
        	if (state_ == State::IDLE) {
            	state_ = State::MOVING;
            	RCLCPP_INFO(this->get_logger(), "START!!");
        	}
    	}
		else if (data == "full_load") {
	        state_ = State::RETURN_2;
	        RCLCPP_INFO(this->get_logger(), " RETURN_2");
	    }
		
		else if (data == "restart") {
	        state_ = State::MOVING;
	        RCLCPP_INFO(this->get_logger(), "RESTART");
		}
	    else if (data == "force_return") {
	        state_ = State::RETURN_1;
	        RCLCPP_INFO(this->get_logger(), "RETURN_1 ");
	    }
	}



    void amcl_callback(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg) {
        current_pose_ = msg->pose.pose;
    }

    void load_camera_params(std::string yaml_path) {
        cv::FileStorage fs(yaml_path, cv::FileStorage::READ);
        if (fs.isOpened()) {
            cv::FileNode cm_node = fs["camera_matrix"];
            std::vector<double> cm_data;
            cm_node["data"] >> cm_data;
            cameraMatrix_ = cv::Mat(3, 3, CV_64F, cm_data.data()).clone();

            cv::FileNode dc_node = fs["distortion_coefficients"];
            std::vector<double> dc_data;
            dc_node["data"] >> dc_data;
            distCoeffs_ = cv::Mat(1, 5, CV_64F, dc_data.data()).clone();
        } else {
            cameraMatrix_ = (cv::Mat_<double>(3,3) << 584.06, 0, 327.48, 0, 584.60, 264.12, 0, 0, 1);
            distCoeffs_ = (cv::Mat_<double>(1,5) << 0.23, -0.48, 0.01, 0.01, 0.0);
        }
    }

    void timer_callback() {
        cv::Mat frame;
        if (!cap_->read(frame)) return;

        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners;
        cv::aruco::detectMarkers(frame, dictionary_, corners, ids, detectorParams_);
        
        geometry_msgs::msg::Twist twist_msg;
        float markerLength = 0.05;
		

		if (state_ == State::IDLE) {
            if (!ids.empty()) {
				if(!first_pose_saved_){
                	start_pose_ = current_pose_;
					first_pose_saved_=true;
				}
                //state_ = State::MOVING;
            }
        } 
        else if (state_ == State::MOVING) {
			if (!ids.empty()) {
			    cv::aruco::drawDetectedMarkers(frame, corners, ids);
			
			    for (size_t i = 0; i < ids.size(); ++i) {
			
			        std::vector<cv::Point3f> objPoints = {
			            {-markerLength/2.f,  markerLength/2.f, 0},
			            { markerLength/2.f,  markerLength/2.f, 0},
			            { markerLength/2.f, -markerLength/2.f, 0},
			            {-markerLength/2.f, -markerLength/2.f, 0}
			        };
			
			        cv::Vec3d rvec, tvec;
			        cv::solvePnP(objPoints, corners[i], cameraMatrix_, distCoeffs_, rvec, tvec);
			        cv::drawFrameAxes(frame, cameraMatrix_, distCoeffs_, rvec, tvec, 0.03);
			
			        double x = tvec[0];
			        double y = tvec[1];
			        double z = tvec[2];
			
			        double distance = std::sqrt(x*x + y*y + z*z);
			
			        std::string dist_text = "Distance 3D: " + std::to_string(distance).substr(0,5);
			
			        cv::putText(frame,
			                    dist_text,
			                    cv::Point(30,30),
			                    cv::FONT_HERSHEY_SIMPLEX,
			                    0.7,
			                    cv::Scalar(0,0,255),
			                    2);
			
			        if (i == 0) {
			            twist_msg.angular.z = -0.5 * x;
			            twist_msg.linear.x = (distance > 0.2) ? 0.05 : 0.0;
			        }
			    }
					cmd_vel_pub_->publish(twist_msg);
				}else{
				    //twist_msg.linear.x = 0.0;
				    //twist_msg.angular.z = 0.0;
					geometry_msgs::msg::Twist stop_msg;
            		cmd_vel_pub_->publish(stop_msg);
				}
 		       	//cmd_vel_pub_->publish(twist_msg);
			}

        else if (state_ == State::RETURN_1 || state_ == State::RETURN_2) {
            static bool nav_sent = false;
            if (!nav_sent) {
                //twist_msg.angular.z = 0.6;
				geometry_msgs::msg::Twist stop_msg;
				stop_msg.linear.x = 0.0;
				stop_msg.angular.z = 0.0;
            	cmd_vel_pub_->publish(stop_msg);

                auto goal = geometry_msgs::msg::PoseStamped();
                goal.header.stamp = this->now();
                goal.header.frame_id = "map"; 
                goal.pose = start_pose_;
                goal_pub_->publish(goal);
                nav_sent = true;
            }

            double d = std::hypot(start_pose_.position.x - current_pose_.position.x, 
                                  start_pose_.position.y - current_pose_.position.y);
            if (d < 0.25) {
                nav_sent = false;
                if (state_ == State::RETURN_1) {
                    state_ = State::IDLE;
                    status_pub_->publish(std_msgs::msg::String().set__data("task_finished"));
                } else {
                    wait_time_ = this->now();
                    state_ = State::WAIT;
                }
            }
        } 
        else if (state_ == State::WAIT) {
            if ((this->now() - wait_time_) > 3s) state_ = State::MOVING;
        }

        //cmd_vel_pub_->publish(twist_msg);
        auto img_msg = cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", frame).toImageMsg();
        image_pub_->publish(*img_msg);
    }

    State state_;
    rclcpp::Time wait_time_;
    std::shared_ptr<cv::VideoCapture> cap_;
    cv::Mat cameraMatrix_, distCoeffs_;
    cv::Ptr<cv::aruco::Dictionary> dictionary_;
    cv::Ptr<cv::aruco::DetectorParameters> detectorParams_;
    geometry_msgs::msg::Pose current_pose_, start_pose_;

    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr goal_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr signal_sub_;
    rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr amcl_sub_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<CameraArucoNode>());
    rclcpp::shutdown();
    return 0;
}
