import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from std_msgs.msg import String, Float32MultiArray
from enum import Enum
import onnxruntime as ort
import numpy as np
import cv2
import math
import threading
import time

class State(Enum):
    IDLE = 0
    MOVING = 1
    WORK_WAIT = 2
    WORKING = 3
    PAUSED = 4
    RETURN_HOME = 5

class WaffleController(Node):

    def __init__(self):
        super().__init__('waffle_controller')

        # 상태
        self.state = State.IDLE
        self.prev_state = None
        # 현재 위치
        self.current_x = 0.9869987186260121
        self.current_y = 0.19443810904149372
        self.current_yaw = 0.0
        # 작업 타겟 위치
        self.check_x = 320.0
        self.check_tolerance = 40
        # 타겟 전달 위치
        self.target_x = 0.0
        self.target_y = 0.0
        self.target_w = 0.0
        self.target_h = 0.0
        # 작업 종료 거리
        self.travel_distance = 2.0100
        # 복귀 위치
        self.home_x = None
        self.home_y = None
        # 허용오차
        self.position_tolerance = 0.1

        # 모델
        self.session = ort.InferenceSession(
            "/home/waf/tomato.onnx",
            providers=['CPUExecutionProvider'],
            sess_options=ort.SessionOptions()
        )
        self.session.set_providers(
            ['CPUExecutionProvider'],
            [{"intra_op_num_threads": 2}]
        )
        self.input_name = self.session.get_inputs()[0].name
        self.cap = cv2.VideoCapture(0)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
        # detection 결과
        self.latest_detection = None
        self.detection_lock = threading.Lock()
        # 추론 스레드 실행
        self.inference_thread = threading.Thread(
            target = self.inference_loop,
            daemon = True
        )
        self.inference_thread.start()

        # Publisher
        self.cmd_pub = self.create_publisher(Twist, '/cmd_vel', 10)
        self.waffle_command_pub = self.create_publisher(String, '/waffle_command', 10)
        self.target_pub = self.create_publisher(Float32MultiArray, '/tomato_target', 10)

        # Qt구독
        self.qt_sub = self.create_subscription(
            String, '/qt_command', self.qt_callback, 10)
        self.burgur_command_sub = self.create_subscription(
            String, '/burger_command', self.burger_command_callback, 10)
        # 매니플레이트 구독
        self.mani_command_sub = self.create_subscription(
            String, '/mani_command', self.mani_command_callback, 10)
        # 좌표 구독
        self.pose_sub = self.create_subscription(
            Odometry, '/odom', self.pose_callback, 10)

        self.timer = self.create_timer(0.1, self.main_loop)

        self.get_logger().info("Waffle Controller Started")

    # Qt 명령 처리
    def qt_callback(self, msg):
        self.get_logger().info(f"QT ORDER CHANGE STATE : {msg.data}")
        cmd = msg.data

        if cmd == "start" and self.state == State.IDLE:
            self.home_x = self.current_x
            self.home_y = self.current_y
            self.change_state(State.MOVING)

        elif cmd == "pause" and self.state != State.PAUSED:
            self.change_state(State.PAUSED)

        elif cmd == "resume" and self.state == State.PAUSED:
            self.change_state(self.prev_state)

        elif cmd == "stop":
            self.change_state(State.RETURN_HOME)
        
        elif cmd == "done":
            self.work_done_publish()
            self.change_state(State.RETURN_HOME)

    # 버거 응답 완료
    def burger_command_callback(self, msg):
        if msg.data == 'burger_response' and self.state == State.WORK_WAIT:
            self.change_state(State.WORKING)

    # 매니퓰레이터 완료
    def mani_command_callback(self, msg):
        if msg.data == 'grap_done' and self.state == State.WORKING:
            self.change_state(State.MOVING)

    # 좌표 콜백
    def pose_callback(self, msg):
        self.current_x = msg.pose.pose.position.x
        self.current_y = msg.pose.pose.position.y
        q = msg.pose.pose.orientation
        self.current_yaw = self.quaternion_to_yaw(q)
    
    def quaternion_to_yaw(self, q):
        siny = 2.0 * (q.w * q.z + q.x * q.y)
        cosy = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
        return math.atan2(siny, cosy)

    # 상태 변경 처리 함수
    def change_state(self, newState):
        if newState == self.state:
            return

        self.prev_state = self.state
        self.state = newState

        if self.state == State.IDLE:
            self.stop_robot()
            
        elif self.state == State.MOVING:
            self.move_forward()

        elif self.state == State.WORK_WAIT:
            with self.detection_lock:
                self.latest_detection = None
            self.stop_robot()
            self.check_burger()

        elif self.state == State.WORKING:
            with self.detection_lock:
                self.latest_detection = None
            self.work_manipulator()

        elif self.state == State.PAUSED:
            self.stop_robot()

        elif self.state == State.RETURN_HOME:
            self.return_home()

        self.get_logger().info(f"CHANGE STATE : {newState}")

    # 상태 확인 루프
    def main_loop(self):
        if self.state == State.MOVING:
            with self.detection_lock:
                detection = self.latest_detection

            if detection is not None:
                twist = Twist()
                twist.linear.x = 0.03
                self.cmd_pub.publish(twist)

                x, y, w, h, conf = detection
                self.get_logger().info(f"x : {x} / y : {y} / w : {w} / h : {h}")

                center_x = float(x + w / 2)
                center_y = float(y + h / 2)

                if abs(center_x - self.check_x) <= self.check_tolerance:
                    self.target_x = float(x)
                    self.target_y = float(y)
                    self.target_w = float(w)
                    self.target_h = float(h)
                    self.change_state(State.WORK_WAIT)

            #elif self.check_end_position():
            #    self.work_done_publish()
            #    self.change_state(State.RETURN_HOME)
        
        elif self.state == State.RETURN_HOME:
            if self.check_home_position():
                self.change_state(State.IDLE)
                self.home_arrive()

    # 전진
    def move_forward(self):
        self.get_logger().info("GO")
        twist = Twist()
        twist.linear.x = 0.05
        self.cmd_pub.publish(twist)

    # 정지
    def stop_robot(self):
        self.get_logger().info("STOP")
        twist = Twist()
        self.cmd_pub.publish(twist)

    # 복귀
    def return_home(self):
        self.get_logger().info("RETURN")
        twist = Twist()
        twist.linear.x = -0.05
        self.cmd_pub.publish(twist)

    # 버거 작업 가능상태 확인
    def check_burger(self):
        self.get_logger().info("BURGER CHECK SEND")
        msg = String()
        msg.data = "burger_check"
        self.waffle_command_pub.publish(msg)

    # 매니퓰레이터 작동
    def work_manipulator(self):
        self.get_logger().info("ACTIVE MANIPULATOR")
        msg = Float32MultiArray()
        msg.data = [self.target_x, self.target_y, self.target_w, self.target_h]
        self.target_pub.publish(msg)

    # 작업 완료
    def work_done_publish(self):
        msg = String()
        msg.data = "waffle_work_done"
        self.waffle_command_pub.publish(msg)

    # 복귀 완료
    def home_arrive(self):
        msg = String()
        msg.data = "idle"
        self.waffle_command_pub.publish(msg)

    # 종료 위치 체크
    def check_end_position(self):
        if self.home_x is None:
            return False
        
        distance = math.sqrt(
            (self.current_x - self.home_x) ** 2 +
            (self.current_y - self.home_y) ** 2
        )

        return distance >= self.travel_distance

    # 복귀 위치 체크
    def check_home_position(self):
        if self.home_x is None:
            return False

        distance = math.sqrt(
            (self.current_x - self.home_x) ** 2 +
            (self.current_y - self.home_y) ** 2
        )

        return distance < self.position_tolerance

    # 토마토 감지
    def inference_loop(self):
        while rclpy.ok():
            if self.state != State.MOVING:
               time.sleep(0.1)
               continue
            ret, frame = self.cap.read()
            if not ret:
                continue
            
            orig_h, orig_w = frame.shape[:2]
            img = cv2.resize(frame, (320, 320))
            img = img[:, :, ::-1].transpose(2, 0, 1)  # BGR → RGB
            img = np.expand_dims(img, axis=0)
            img = img.astype(np.float32) / 255.0

            
            outputs = self.session.run(None, {self.input_name: img})
            predictions = outputs[0][0].T

            best_detection = None

            for det in predictions:
                x, y, w, h = det[:4]
                class_scores = det[4:]

                class_id = int(np.argmax(class_scores))
                conf = float(class_scores[class_id])

                if conf >= 0.7 and class_id == 0:
                    scale_x = orig_w / 320.0
                    scale_y = orig_h / 320.0

                    min_x = x - w / 2
                    min_y = y - h / 2
                    min_x *= scale_x
                    min_y *= scale_y
                    w *= scale_x
                    h *= scale_y

                    best_detection = (min_x, min_y, w, h, conf)
                    break
        
            with self.detection_lock:
                self.latest_detection = best_detection
            
            time.sleep(0.01)

def main(args=None):
    rclpy.init(args=args)
    node = WaffleController()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
