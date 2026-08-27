import os

# --- MQTT broker (running on your VPS, e.g. Mosquitto) ---
MQTT_BROKER = os.getenv("GF_MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("GF_MQTT_PORT", "1883"))
MQTT_USERNAME = os.getenv("GF_MQTT_USER", "")
MQTT_PASSWORD = os.getenv("GF_MQTT_PASS", "")

# --- MQTT topics (shared contract between server and both ESP32 firmwares) ---
TOPIC_CAM_IMAGE = "greenflow/cam1/image"            # binary JPEG payload from ESP32-CAM
TOPIC_CAM_STATUS = "greenflow/cam1/status"          # "online"/"offline" (LWT)
TOPIC_LIGHT_CONTROL = "greenflow/control/lights"    # JSON command -> ESP32-C6
TOPIC_C6_STATUS = "greenflow/c6/status"              # "online"/"offline" (LWT)

# --- MongoDB ---
MONGO_URI = os.getenv("GF_MONGO_URI", "mongodb://localhost:27017")
MONGO_DB = os.getenv("GF_MONGO_DB", "greenflow")

# --- YOLOv8n ncnn model ---
MODEL_PATH = os.getenv("GF_MODEL_PATH", "yolov8n_ncnn_model")
VEHICLE_CLASSES = {"car", "motorcycle", "bus", "truck", "bicycle"}  # COCO class names to count
CONF_THRESHOLD = 0.35

# --- Adaptive traffic light logic ---
MIN_GREEN_SECONDS = 10
MAX_GREEN_SECONDS = 60
BASE_GREEN_SECONDS = 15
SECONDS_PER_VEHICLE = 2.5
YELLOW_SECONDS = 3
RED_SECONDS = 5  # minimum all-red / pedestrian buffer

# --- Flask ---
FLASK_HOST = os.getenv("GF_HOST", "0.0.0.0")
FLASK_PORT = int(os.getenv("GF_PORT", "5000"))
