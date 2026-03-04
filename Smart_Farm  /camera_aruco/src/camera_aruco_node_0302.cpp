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
#include "std_msgs/msg/bool.hpp"

using namespace std::chrono_literals;

class CameraArucoNode : public rclcpp::Node {
public:
    enum class State { IDLE, GO_TO_WAFFLE, MOVING, RETURN_1, RETURN_2, RETURN_3, WAIT, RESTART };

    CameraArucoNode() : Node("camera_aruco_node"), state_(State::IDLE) {
        cap_ = std::make_shared<cv::VideoCapture>(-1);
        auto amcl_qos = rclcpp::QoS(rclcpp::KeepLast(1)).transient_local();
		//publisher
        //image_pub_ = this->create_publisher<sensor_msgs::msg::Image>("burger/aruco_image", 10);
        cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
        goal_pub_ = this->create_publisher<geometry_msgs::msg::PoseStamped>("/goal_pose", 10);
        status_pub_ = this->create_publisher<std_msgs::msg::String>("/work_status", 10);
		burger_command_pub_ = this->create_publisher<std_msgs::msg::String>("/burger_command", 10);
		init_pose_pub_ = this->create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>("/initialpose", 10);


		//subscription
        signal_sub_ = this->create_subscription<std_msgs::msg::String>(
            "/qt_command", 10, std::bind(&CameraArucoNode::signal_callback, this, std::placeholders::_1));
        amcl_sub_ = this->create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
            "/amcl_pose", amcl_qos, std::bind(&CameraArucoNode::amcl_callback, this, std::placeholders::_1));

		waffle_command_sub_ = this->create_subscription<std_msgs::msg::String>(
    	"/waffle_command", 10, std::bind(&CameraArucoNode::waffle_command_callback, this, std::placeholders::_1));

		//waffle_done_sub_ = this->create_subscription<std_msgs::msg::Bool>(
		//"/waffle_work_done", 10, [this](const std_msgs::msg::Bool::SharedPtr msg) {
		//	if (msg->data && state_ == State::MOVING) {
		//		RCLCPP_INFO(this->get_logger(), "Waffle work done! Returning Home (RETURN_3)...");
		//		state_ = State::RETURN_3;
		//		full_load_count_ = 0;
		//		nav_sent_ = false;
		//		}
		//	});

		//full_load_sub_ = this->create_subscription<std_msgs::msg::Bool>(
    	//"/grasp_done", 10, [this](const std_msgs::msg::Bool::SharedPtr msg) {
        //if (msg->data) {
        //    full_load_count_++;
        //    RCLCPP_INFO(this->get_logger(), "Item Loaded! Current Count: %d/8", full_load_count_);

        //    if (full_load_count_ >= 8 && state_ == State::MOVING) {
        //        RCLCPP_INFO(this->get_logger(), "Full load reached (8 items)! Returning...");
	    //	    RCLCPP_INFO(this->get_logger(), " RETURN_2");
        //        state_ = State::RETURN_2;
        //        nav_sent_ = false;
        //        full_load_count_ = 0;
        //    }
        //}
    	//});

		dictionary_ = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_1000);
        detectorParams_ = cv::aruco::DetectorParameters::create();
        
        load_camera_params("/home/ubuntu/.ros/camera_info/mmal_service_16.1.yaml");

        timer_ = this->create_wall_timer(100ms, std::bind(&CameraArucoNode::timer_callback, this));
    }

private:
	bool first_pose_saved_ = false;
	bool nav_sent_ = false;
	bool has_saved_moving_pose_ = false;
	double current_marker_dist_ = 999.9;
	int full_load_count_ = 0;

	double Kp_lin = 0.4;  //linear p
    double Kd_lin = 0.05; //linear d
    double Kp_ang = 1.2;  //angular p
    double Kd_ang = 0.1;  //angular d

    double last_dist_error = 0.0;
    double last_x_error = 0.0;


	void signal_callback(const std_msgs::msg::String::SharedPtr msg) {
	    std::string cmd = msg->data;
	    
	    if (cmd.find("burger/") == 0) {
	        cmd = cmd.substr(7);
	    }
	
	    cmd.erase(0, cmd.find_first_not_of(" \t\n\r"));
	    cmd.erase(cmd.find_last_not_of(" \t\n\r") + 1);
	
	    RCLCPP_INFO(this->get_logger(), "[%s]", cmd.c_str());
	
		if (cmd == "start") {
    	
			RCLCPP_INFO(this->get_logger(), "Moving to Waffle Start Pose first...");
        
        	auto goal = geometry_msgs::msg::PoseStamped();
        	goal.header.stamp = this->now();
        	goal.header.frame_id = "map";
        	goal.pose.position.x = 0.9869987186260121;
        	goal.pose.position.y = 0.19443810904149372;
        	goal.pose.orientation.z = -0.9990506253898861;
        	goal.pose.orientation.w = 0.043564296253669525;
        	
        	goal_pub_->publish(goal);
        	
        	state_ = State::GO_TO_WAFFLE;
        	nav_sent_ = true;
			has_saved_moving_pose_=false;
		}
		else if (cmd == "ready"){
			auto init_pose = geometry_msgs::msg::PoseWithCovarianceStamped();
			init_pose.header.stamp = this->now();
    		init_pose.header.frame_id = "map";
			//init_pose.pose.position.x = 0.9869987186260121;
        	//init_pose.pose.position.y = 0.19443810904149372;
        	//init_pose.pose.orientation.z = -0.9990506253898861;
        	//init_pose.pose.orientation.w = 0.043564296253669525;
			init_pose.pose.pose.position.x = 1.1501020291584654;
            init_pose.pose.pose.position.y = -0.5566899787970928;
            init_pose.pose.pose.position.z = 0.0;
            init_pose.pose.pose.orientation.z = 0.7100985734384994;
            init_pose.pose.pose.orientation.w = 0.7041022766619975;
        	init_pose_pub_->publish(init_pose);

		}
		else if (cmd == "finish") {
	        if (has_saved_moving_pose_) {
	            RCLCPP_INFO(this->get_logger(), "RESTART: Navigating back to saved work pose...");
	            
	            auto goal = geometry_msgs::msg::PoseStamped();
	            goal.header.stamp = this->now();
	            goal.header.frame_id = "map";
	            goal.pose = last_moving_pose_;
	
	            goal_pub_->publish(goal);
	            state_ = State::RESTART;
	            nav_sent_ = true;
	        } else {
	            RCLCPP_INFO(this->get_logger(), "No saved pose. Switching to MOVING immediately.");
	            state_ = State::MOVING;
	            nav_sent_ = false;
	        }
	    }
	

		//else if (data == "full_load") {
	    //    state_ = State::RETURN_2;
	    //    RCLCPP_INFO(this->get_logger(), " RETURN_2");
	    //}
		
	    else if (cmd == "stop") {
	        state_ = State::RETURN_1;
	        RCLCPP_INFO(this->get_logger(), "RETURN_1");
	    }

		else if (cmd == "return"){
            if (state_ == State::MOVING) {
				last_moving_pose_ = current_pose_;
                has_saved_moving_pose_ = true;
                RCLCPP_INFO(this->get_logger(), "Full load reached (3 items)! Returning...");
	    	    RCLCPP_INFO(this->get_logger(), " RETURN_2");
                state_ = State::RETURN_2;
                nav_sent_ = false;
                full_load_count_ = 0;
            }
       	 }
	   }


    void amcl_callback(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg) {
        current_pose_ = msg->pose.pose;
    }


	void waffle_command_callback(const std_msgs::msg::String::SharedPtr msg) {
		std::string cmd = msg->data;	    
		RCLCPP_INFO(this->get_logger(), "Waffle Command Received: [%s]", cmd.c_str());


		if (cmd == "burger_check") {
	        auto response = std_msgs::msg::String();
	        
	        if (state_ == State::MOVING && current_marker_dist_ < 0.2) {
	            response.data = "burger_response";
	            RCLCPP_INFO(this->get_logger(), "Burger is READY! Sending: %s", response.data.c_str());
	        } else {
	            response.data = "not_ready";
				RCLCPP_WARN(this->get_logger(), "Burger NOT ready. Sending: %s (Dist: %.2f)", response.data.c_str(), current_marker_dist_);
	        }
	        burger_command_pub_->publish(response);
	    }

		else if (cmd == "waffle_work_done" && state_ == State::MOVING) {
                RCLCPP_INFO(this->get_logger(), "Received 'waffle_work_done'. Returning Home (RETURN_3)...");
                state_ = State::RETURN_3;
                full_load_count_ = 0;
                nav_sent_ = false;
            } else {
                RCLCPP_WARN(this->get_logger(), "Received 'waffle_work_done' but not in MOVING state.");
            }
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
            if (!ids.empty()&& !first_pose_saved_) {
                	start_pose_ = current_pose_;
					first_pose_saved_=true;
				}
        }
		else if (state_ == State::GO_TO_WAFFLE) {
        	double d = std::hypot(0.9869987186260121 - current_pose_.position.x, 
        	                      0.19443810904149372 - current_pose_.position.y);
			RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Distance to Waffle: %.3f m", d);

        	if (d < 0.5) {
        	    RCLCPP_INFO(this->get_logger(), "Arrived at Waffle Pose. Switching to MOVING (Marker Tracking)...");
        	    state_ = State::MOVING;
        	    nav_sent_ = false;
        	}
        	//goto publish_image;
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
					current_marker_dist_ = distance;
			        std::string dist_text = "Distance 3D: " + std::to_string(distance).substr(0,5);
			
			        cv::putText(frame,
			                    dist_text,
			                    cv::Point(30,30),
			                    cv::FONT_HERSHEY_SIMPLEX,
			                    0.7,
			                    cv::Scalar(0,0,255),
			                    2);
			
			        if (i == 0) {
			            twist_msg.angular.z = -1.0 * x;
						if (distance > 0.2) {
                    		double speed = 0.05 + (distance - 0.2) * 0.25; 
                    		twist_msg.linear.x = std::min(speed, 0.15); 
                		} else {
                    		twist_msg.linear.x = 0.0;
                		}

			        }
			    }
					cmd_vel_pub_->publish(twist_msg);
				}else{
				    //twist_msg.linear.x = 0.0;
				    //twist_msg.angular.z = 0.0;
					geometry_msgs::msg::Twist stop_msg;  //0 init
            		cmd_vel_pub_->publish(stop_msg);
				}
			}

        else if (state_ == State::RETURN_1 || state_ == State::RETURN_2 || state_ == State::RETURN_3) {
            if (!nav_sent_) {
				geometry_msgs::msg::Twist stop_msg;
            	cmd_vel_pub_->publish(stop_msg);

                auto goal = geometry_msgs::msg::PoseStamped();
                goal.header.stamp = this->now();
                goal.header.frame_id = "map"; 
                
				//goal.pose = start_pose_;
				goal.pose.position.x = 1.1501020291584654;
                goal.pose.position.y = -0.5566899787970928;
                goal.pose.position.z = 0.0;
                goal.pose.orientation.z = 0.7100985734384994;
                goal.pose.orientation.w = 0.7041022766619975;
                // ---------------------------------------

                goal_pub_->publish(goal);
                nav_sent_ = true;
                RCLCPP_INFO(this->get_logger(), "Returning to Hard-coded Home Pose...");
            }

            double d = std::hypot(1.1501020291584654 - current_pose_.position.x, 
                                  -0.5566899787970928 - current_pose_.position.y);

            if (d < 0.7) {
                nav_sent_ = false;
                if (state_ == State::RETURN_1|| state_ == State::RETURN_3) {
                    state_ = State::IDLE;
                    burger_command_pub_->publish(std_msgs::msg::String().set__data("idle"));
                	RCLCPP_INFO(this->get_logger(), "idle idle...");
                } else {
                    wait_time_ = this->now();
                    state_ = State::WAIT;
                }
            }
			//goto publish_image;
        } 
        else if (state_ == State::WAIT) {
            if ((this->now() - wait_time_) > 3s) {
                //status_pub_->publish(std_msgs::msg::String().set__data("finished"));
                burger_command_pub_->publish(std_msgs::msg::String().set__data("finish"));
				state_ = State::RESTART;
			}
        }

		else if (state_ == State::RESTART) {
        double d = std::hypot(last_moving_pose_.position.x - current_pose_.position.x, 
                              last_moving_pose_.position.y - current_pose_.position.y);
        
        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Distance to saved pose: %.3f m", d);
        
        if (d < 0.5) { 
            RCLCPP_INFO(this->get_logger(), "Arrived at saved pose. Switching to MOVING.");
            state_ = State::MOVING;
            nav_sent_ = false;
        	}
    	}
		//publish_image:
        //cmd_vel_pub_->publish(twist_msg);
        	//auto img_msg = cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", frame).toImageMsg();
        	//image_pub_->publish(*img_msg);
    }

    State state_;
    rclcpp::Time wait_time_;
    std::shared_ptr<cv::VideoCapture> cap_;
    cv::Mat cameraMatrix_, distCoeffs_;
    cv::Ptr<cv::aruco::Dictionary> dictionary_;
    cv::Ptr<cv::aruco::DetectorParameters> detectorParams_;
    geometry_msgs::msg::Pose current_pose_, start_pose_;
	geometry_msgs::msg::Pose last_moving_pose_;
    //rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr goal_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr burger_command_pub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr init_pose_pub_;
    
	rclcpp::Subscription<std_msgs::msg::String>::SharedPtr signal_sub_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr waffle_command_sub_;
    //rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr waffle_done_sub_ ;
	//rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr full_load_sub_;
    rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr amcl_sub_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<CameraArucoNode>());
    rclcpp::shutdown();
    return 0;
}
