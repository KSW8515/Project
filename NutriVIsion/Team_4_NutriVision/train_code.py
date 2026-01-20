# 데이터 학습 코드
import os
import yaml
from ultralytics import YOLO
# --- 2. 데이터셋 경로 설정 ---
# 이전 단계에서 압축을 푼 경로를 그대로 사용합니다.
base_path = '/content/food_dataset/Food_data'
data_path = base_path # 데이터셋의 루트 경로
# --- 3. YOLO 학습을 위한 data.yaml 파일 생성 ---
print("\n학습 설정 파일(food_data.yaml)을 생성합니다...")
# 클래스 이름 목록 (이전과 동일)
class_names = [
    '초밥', '라면', '햄버거', '파스타', '샌드위치',
    '김치볶음밥', '치킨', '타코', '카레라이스', '스테이크',
    '피자', '가츠동', '만두', '카케우동', '샐러드',
    '비빔밥', '떡볶이', '김밥', '소금빵', '김치찌개'
]
# YAML 파일 데이터 구조
data_yaml = {
    'path': data_path,  # 데이터셋 루트 경로
    'train': 'images/train',  # train 이미지 폴더 (루트 경로 기준)
    'val': 'images/val',    # val 이미지 폴더 (루트 경로 기준)
    'nc': len(class_names),
    'names': class_names
}
# YAML 파일 작성
yaml_path = os.path.join(base_path, 'food_data.yaml')
with open(yaml_path, 'w', encoding='utf-8') as f:
    yaml.dump(data_yaml, f, allow_unicode=True, sort_keys=False)
print(f":흰색_확인_표시: YAML 파일 생성 완료: {yaml_path}")
with open(yaml_path, 'r', encoding='utf-8') as f:
    print("--- YAML 파일 내용 ---")
    print(f.read())
    print("----------------------")
# --- 4. YOLOv8 모델 학습 시작 ---
print("\nYOLOv8 모델 학습을 시작합니다...")
# 사전 학습된 모델 로드 (yolov8s.pt는 nano보다 약간 더 크고 성능이 좋습니다)
model = YOLO('yolov8s.pt')
# 모델 학습 실행 (주요 옵션들을 명확하게 설정)
results = model.train(
    # --- 데이터 및 모델 설정 ---
    data=yaml_path,         # 방금 생성한 YAML 파일 경로
    project="food_detection_results", # 결과가 저장될 프로젝트 폴더
    name="food_model_s_100epochs",    # 이번 학습의 이름
    exist_ok=True,          # 동일 이름의 폴더가 있어도 덮어쓰기 허용
    # --- 학습 하이퍼파라미터 ---
    epochs=100,             # 전체 데이터셋 반복 학습 횟수
    imgsz=640,              # 학습 이미지 크기
    batch=16,               # 한 번에 처리할 이미지 수 (Colab T4 GPU는 16이 안정적)
    device=0,               # 사용할 GPU 장치 번호 (0번 GPU 사용)
    augment=True,           # 데이터 증강 기능 사용 (이미지를 변형시켜 모델 성능 향상)
    iou=0.5,                # IOU (Intersection over Union) 임계값
    # --- 출력 및 기타 설정 ---
    verbose=True            # 학습 과정 상세히 출력
)
print("\n:짠: 학습이 성공적으로 완료되었습니다!")
print(f"결과는 '{results.save_dir}' 폴더에서 확인하실 수 있습니다.")
